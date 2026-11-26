#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#define SECURITY_WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <security.h>
#include <schannel.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "easywsclient_schannel.hpp"

namespace {

    int log_error(const char* format, ...)
    {
        va_list vl;
        va_start(vl, format);
        const int written = vfprintf(stderr, format, vl);
        va_end(vl);
        return written;
    }

    std::wstring Utf8ToWide(const std::string& s)
    {
        if (s.empty()) return {};
        const int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                             static_cast<int>(s.size()),
                                             nullptr, 0);
        if (wlen <= 0) return {};
        std::wstring out;
        out.resize(static_cast<size_t>(wlen));
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                            static_cast<int>(s.size()),
                            out.data(), wlen);
        return out;
    }

    std::string Base64(const BYTE* data, size_t len)
    {
        static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        for (size_t i = 0; i < len; i += 3) {
            const uint32_t a = data[i];
            const uint32_t b = i + 1 < len ? data[i + 1] : 0;
            const uint32_t c = i + 2 < len ? data[i + 2] : 0;
            const uint32_t n = (a << 16) | (b << 8) | c;
            out.push_back(kAlphabet[(n >> 18) & 63]);
            out.push_back(kAlphabet[(n >> 12) & 63]);
            out.push_back(i + 1 < len ? kAlphabet[(n >> 6) & 63] : '=');
            out.push_back(i + 2 < len ? kAlphabet[n & 63] : '=');
        }
        return out;
    }

    void RandomBytes(BYTE* out, size_t len)
    {
        std::random_device device;
        for (size_t i = 0; i < len; ++i) {
            out[i] = static_cast<BYTE>(device());
        }
    }

    std::string MaskedFrame(BYTE opcode, const char* payload, size_t len)
    {
        std::string out;
        out.push_back(static_cast<char>(0x80 | opcode));
        if (len < 126) {
            out.push_back(static_cast<char>(0x80 | len));
        }
        else if (len <= 0xFFFF) {
            out.push_back(static_cast<char>(0x80 | 126));
            out.push_back(static_cast<char>(len >> 8));
            out.push_back(static_cast<char>(len & 0xFF));
        }
        else {
            out.push_back(static_cast<char>(0x80 | 127));
            for (int shift = 56; shift >= 0; shift -= 8) {
                out.push_back(static_cast<char>((static_cast<uint64_t>(len) >> shift) & 0xFF));
            }
        }
        BYTE mask[4];
        RandomBytes(mask, sizeof(mask));
        out.append(reinterpret_cast<const char*>(mask), sizeof(mask));
        for (size_t i = 0; i < len; ++i) {
            out.push_back(static_cast<char>(payload[i] ^ mask[i % 4]));
        }
        return out;
    }

    class _SchannelWebSocket : public easywsclient::WebSocket {
    public:
        _SchannelWebSocket() = default;

        ~_SchannelWebSocket() override
        {
            m_running = false;
            if (m_socket != INVALID_SOCKET) {
                shutdown(m_socket, SD_BOTH);
                closesocket(m_socket);
                m_socket = INVALID_SOCKET;
            }
            if (m_reader.joinable()) {
                m_reader.join();
            }
            if (m_has_ctx) {
                DeleteSecurityContext(&m_ctx);
            }
            if (m_has_cred) {
                FreeCredentialsHandle(&m_cred);
            }
        }

        bool open(const std::string& host, int port, bool is_ssl, const std::string& path,
                  const easywsclient::HeaderKeyValuePair& additional_headers, const std::string& origin)
        {
            if (!connectSocket(host, port)) {
                log_error("easywsclient: TCP connect to %s:%d failed\n", host.c_str(), port);
                return false;
            }
            if (is_ssl && !handshakeTls(host)) {
                return false;
            }

            BYTE key_bytes[16];
            RandomBytes(key_bytes, sizeof(key_bytes));
            const std::string port_suffix = (port == 443 || port == 80) ? std::string() : ":" + std::to_string(port);
            std::string request = "GET " + path + " HTTP/1.1\r\nHost: " + host + port_suffix +
                                  "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " +
                                  Base64(key_bytes, sizeof(key_bytes)) + "\r\nSec-WebSocket-Version: 13\r\n";
            if (!origin.empty()) {
                request += "Origin: " + origin + "\r\n";
            }
            for (const auto& kv : additional_headers) {
                request += kv.first + ": " + kv.second + "\r\n";
            }
            request += "\r\n";
            if (!sendPayload(request.data(), request.size())) {
                log_error("easywsclient: websocket upgrade request failed\n");
                return false;
            }

            while (m_rx_plain.find("\r\n\r\n") == std::string::npos) {
                if (!pumpReceive()) {
                    log_error("easywsclient: no websocket upgrade reply from %s\n", host.c_str());
                    return false;
                }
            }
            const size_t end = m_rx_plain.find("\r\n\r\n") + 4;
            const std::string reply = m_rx_plain.substr(0, end);
            m_rx_plain.erase(0, end);
            if (reply.compare(0, 12, "HTTP/1.1 101") != 0) {
                log_error("easywsclient: websocket upgrade refused: %s\n", reply.substr(0, reply.find("\r\n")).c_str());
                return false;
            }

            DWORD no_timeout = 0;
            setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&no_timeout), sizeof(no_timeout));
            m_state.store(OPEN);
            m_reader = std::thread(&_SchannelWebSocket::readerLoop, this);
            return true;
        }

        readyStateValues getReadyState() const override
        {
            return m_state.load(std::memory_order_acquire);
        }

        void poll(int timeout) override
        {
            if (m_state.load() == CLOSED) {
                if (timeout > 0) {
                    Sleep(static_cast<DWORD>(timeout));
                }
                return;
            }
            if (timeout > 0) {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
                std::unique_lock lk(m_queue_mutex);
                m_queue_cv.wait_until(lk, deadline, [this] {
                    return !m_incoming.empty() || m_state.load() == CLOSED;
                });
            }
        }

        void sendPing() override
        {
            const std::string frame = MaskedFrame(0x9, nullptr, 0);
            std::scoped_lock lk(m_send_mutex);
            sendPayload(frame.data(), frame.size());
        }

        void send(const std::string& message) override
        {
            if (m_state.load() != OPEN) {
                return;
            }
            const std::string frame = MaskedFrame(0x1, message.data(), message.size());
            std::scoped_lock lk(m_send_mutex);
            if (!sendPayload(frame.data(), frame.size())) {
                log_error("easywsclient: websocket send failed\n");
                m_state.store(CLOSED);
                m_queue_cv.notify_all();
            }
        }

        void close() override
        {
            readyStateValues expected = OPEN;
            if (!m_state.compare_exchange_strong(expected, CLOSING)) {
                return;
            }
            const std::string frame = MaskedFrame(0x8, nullptr, 0);
            {
                std::scoped_lock lk(m_send_mutex);
                sendPayload(frame.data(), frame.size());
            }
            shutdown(m_socket, SD_SEND);
        }

        void _dispatch(Callback& callable) override
        {
            std::queue<std::string> drain;
            {
                std::scoped_lock lk(m_queue_mutex);
                drain.swap(m_incoming);
            }
            while (!drain.empty()) {
                callable(drain.front());
                drain.pop();
            }
        }

    private:
        enum class Step { NeedMore, Handled, Closed };

        bool connectSocket(const std::string& host, int port)
        {
            static const bool started = [] {
                WSADATA data{};
                return WSAStartup(MAKEWORD(2, 2), &data) == 0;
            }();
            if (!started) {
                return false;
            }
            addrinfo hints{};
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            addrinfo* list = nullptr;
            const std::string service = std::to_string(port);
            if (getaddrinfo(host.c_str(), service.c_str(), &hints, &list) != 0) {
                return false;
            }
            for (addrinfo* a = list; a && m_socket == INVALID_SOCKET; a = a->ai_next) {
                const SOCKET s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
                if (s == INVALID_SOCKET) {
                    continue;
                }
                if (connect(s, a->ai_addr, static_cast<int>(a->ai_addrlen)) == 0) {
                    m_socket = s;
                }
                else {
                    closesocket(s);
                }
            }
            freeaddrinfo(list);
            if (m_socket == INVALID_SOCKET) {
                return false;
            }
            DWORD handshake_timeout_ms = 10000;
            setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&handshake_timeout_ms), sizeof(handshake_timeout_ms));
            return true;
        }

        bool handshakeTls(const std::string& host)
        {
            SCHANNEL_CRED cred_data{};
            cred_data.dwVersion = SCHANNEL_CRED_VERSION;
            cred_data.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT;
            cred_data.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS;
            TimeStamp expiry{};
            if (AcquireCredentialsHandleW(nullptr, const_cast<SEC_WCHAR*>(L"Microsoft Unified Security Protocol Provider"),
                                          SECPKG_CRED_OUTBOUND, nullptr, &cred_data, nullptr, nullptr, &m_cred, &expiry) != SEC_E_OK) {
                log_error("easywsclient: AcquireCredentialsHandle failed\n");
                return false;
            }
            m_has_cred = true;

            const std::wstring target = Utf8ToWide(host);
            const ULONG flags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY |
                                ISC_RET_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM;
            bool have_ctx = false;
            for (;;) {
                SecBuffer in_bufs[2] = {};
                in_bufs[0].BufferType = SECBUFFER_TOKEN;
                in_bufs[0].pvBuffer = m_rx_raw.data();
                in_bufs[0].cbBuffer = static_cast<ULONG>(m_rx_raw.size());
                in_bufs[1].BufferType = SECBUFFER_EMPTY;
                SecBufferDesc in_desc{SECBUFFER_VERSION, 2, in_bufs};
                SecBuffer out_buf{};
                out_buf.BufferType = SECBUFFER_TOKEN;
                SecBufferDesc out_desc{SECBUFFER_VERSION, 1, &out_buf};
                ULONG attrs = 0;

                const SECURITY_STATUS st = InitializeSecurityContextW(
                    &m_cred, have_ctx ? &m_ctx : nullptr, have_ctx ? nullptr : const_cast<SEC_WCHAR*>(target.c_str()),
                    flags, 0, 0, have_ctx ? &in_desc : nullptr, 0, have_ctx ? nullptr : &m_ctx, &out_desc, &attrs, nullptr);
                if (!have_ctx && (st == SEC_E_OK || st == SEC_I_CONTINUE_NEEDED || st == SEC_E_INCOMPLETE_MESSAGE)) {
                    m_has_ctx = true;
                }
                have_ctx = true;

                if (out_buf.pvBuffer && out_buf.cbBuffer) {
                    const bool sent = sendSocket(static_cast<const char*>(out_buf.pvBuffer), out_buf.cbBuffer);
                    FreeContextBuffer(out_buf.pvBuffer);
                    if (!sent) {
                        return false;
                    }
                }
                if (st == SEC_E_INCOMPLETE_MESSAGE) {
                    if (!receiveRaw(m_rx_raw)) {
                        return false;
                    }
                    continue;
                }
                if (st != SEC_E_OK && st != SEC_I_CONTINUE_NEEDED) {
                    log_error("easywsclient: TLS handshake with %s failed: 0x%08lx\n", host.c_str(), static_cast<unsigned long>(st));
                    return false;
                }
                const size_t extra = in_bufs[1].BufferType == SECBUFFER_EXTRA ? in_bufs[1].cbBuffer : 0;
                m_rx_raw.erase(0, m_rx_raw.size() - extra);
                if (st == SEC_E_OK) {
                    break;
                }
                if (extra == 0 && !receiveRaw(m_rx_raw)) {
                    return false;
                }
            }

            m_tls = true;
            if (QueryContextAttributesW(&m_ctx, SECPKG_ATTR_STREAM_SIZES, &m_sizes) != SEC_E_OK) {
                return false;
            }
            return m_rx_raw.empty() || decryptRaw();
        }

        bool receiveRaw(std::string& buf)
        {
            char chunk[16384];
            const int n = recv(m_socket, chunk, sizeof(chunk), 0);
            if (n <= 0) {
                return false;
            }
            buf.append(chunk, static_cast<size_t>(n));
            return true;
        }

        bool pumpReceive()
        {
            if (!m_tls) {
                return receiveRaw(m_rx_plain);
            }
            if (!receiveRaw(m_rx_raw)) {
                return false;
            }
            return decryptRaw();
        }

        bool decryptRaw()
        {
            while (!m_rx_raw.empty()) {
                SecBuffer bufs[4] = {};
                bufs[0].BufferType = SECBUFFER_DATA;
                bufs[0].pvBuffer = m_rx_raw.data();
                bufs[0].cbBuffer = static_cast<ULONG>(m_rx_raw.size());
                bufs[1].BufferType = SECBUFFER_EMPTY;
                bufs[2].BufferType = SECBUFFER_EMPTY;
                bufs[3].BufferType = SECBUFFER_EMPTY;
                SecBufferDesc desc{SECBUFFER_VERSION, 4, bufs};
                const SECURITY_STATUS st = DecryptMessage(&m_ctx, &desc, 0, nullptr);
                if (st == SEC_E_INCOMPLETE_MESSAGE) {
                    return true;
                }
                if (st != SEC_E_OK) {
                    log_error("easywsclient: TLS decrypt failed: 0x%08lx\n", static_cast<unsigned long>(st));
                    return false;
                }
                std::string extra;
                for (const SecBuffer& b : bufs) {
                    if (b.BufferType == SECBUFFER_DATA && b.pvBuffer) {
                        m_rx_plain.append(static_cast<const char*>(b.pvBuffer), b.cbBuffer);
                    }
                    if (b.BufferType == SECBUFFER_EXTRA && b.pvBuffer) {
                        extra.assign(static_cast<const char*>(b.pvBuffer), b.cbBuffer);
                    }
                }
                m_rx_raw = std::move(extra);
            }
            return true;
        }

        bool sendSocket(const char* data, size_t len)
        {
            while (len) {
                const int n = ::send(m_socket, data, static_cast<int>(len), 0);
                if (n <= 0) {
                    return false;
                }
                data += n;
                len -= static_cast<size_t>(n);
            }
            return true;
        }

        bool sendPayload(const char* data, size_t len)
        {
            if (!m_tls) {
                return sendSocket(data, len);
            }
            size_t offset = 0;
            while (offset < len) {
                const size_t chunk = std::min<size_t>(len - offset, m_sizes.cbMaximumMessage);
                std::vector<char> buf(m_sizes.cbHeader + chunk + m_sizes.cbTrailer);
                SecBuffer bufs[4] = {};
                bufs[0].BufferType = SECBUFFER_STREAM_HEADER;
                bufs[0].pvBuffer = buf.data();
                bufs[0].cbBuffer = m_sizes.cbHeader;
                bufs[1].BufferType = SECBUFFER_DATA;
                bufs[1].pvBuffer = buf.data() + m_sizes.cbHeader;
                bufs[1].cbBuffer = static_cast<ULONG>(chunk);
                memcpy(bufs[1].pvBuffer, data + offset, chunk);
                bufs[2].BufferType = SECBUFFER_STREAM_TRAILER;
                bufs[2].pvBuffer = buf.data() + m_sizes.cbHeader + chunk;
                bufs[2].cbBuffer = m_sizes.cbTrailer;
                bufs[3].BufferType = SECBUFFER_EMPTY;
                SecBufferDesc desc{SECBUFFER_VERSION, 4, bufs};
                if (EncryptMessage(&m_ctx, 0, &desc, 0) != SEC_E_OK) {
                    return false;
                }
                const size_t total = bufs[0].cbBuffer + bufs[1].cbBuffer + bufs[2].cbBuffer;
                if (!sendSocket(buf.data(), total)) {
                    return false;
                }
                offset += chunk;
            }
            return true;
        }

        void readerLoop()
        {
            while (m_running.load()) {
                const Step step = parseFrame();
                if (step == Step::Closed) {
                    break;
                }
                if (step == Step::NeedMore && !pumpReceive()) {
                    break;
                }
            }
            m_state.store(CLOSED);
            m_queue_cv.notify_all();
        }

        Step parseFrame()
        {
            if (m_rx_plain.size() < 2) {
                return Step::NeedMore;
            }
            const auto b0 = static_cast<BYTE>(m_rx_plain[0]);
            const auto b1 = static_cast<BYTE>(m_rx_plain[1]);
            const bool masked = (b1 & 0x80) != 0;
            uint64_t len = b1 & 0x7F;
            size_t header = 2;
            if (len == 126) {
                header = 4;
            }
            else if (len == 127) {
                header = 10;
            }
            if (m_rx_plain.size() < header) {
                return Step::NeedMore;
            }
            if (len == 126) {
                len = (static_cast<uint64_t>(static_cast<BYTE>(m_rx_plain[2])) << 8) | static_cast<BYTE>(m_rx_plain[3]);
            }
            else if (len == 127) {
                len = 0;
                for (size_t i = 2; i < 10; ++i) {
                    len = (len << 8) | static_cast<BYTE>(m_rx_plain[i]);
                }
            }
            const size_t mask_at = header;
            if (masked) {
                header += 4;
            }
            if (len > (64u << 20) || m_rx_plain.size() < header + len) {
                return len > (64u << 20) ? Step::Closed : Step::NeedMore;
            }
            std::string payload = m_rx_plain.substr(header, static_cast<size_t>(len));
            if (masked) {
                for (size_t i = 0; i < payload.size(); ++i) {
                    payload[i] = static_cast<char>(payload[i] ^ m_rx_plain[mask_at + (i % 4)]);
                }
            }
            m_rx_plain.erase(0, header + static_cast<size_t>(len));
            return handleFrame(static_cast<BYTE>(b0 & 0x0F), (b0 & 0x80) != 0, std::move(payload));
        }

        Step handleFrame(BYTE opcode, bool fin, std::string payload)
        {
            switch (opcode) {
                case 0x0:
                case 0x1:
                case 0x2:
                    if (opcode != 0x0) {
                        m_fragment.clear();
                    }
                    m_fragment += payload;
                    if (fin) {
                        enqueue(std::move(m_fragment));
                        m_fragment.clear();
                    }
                    return Step::Handled;
                case 0x8: {
                    const std::string frame = MaskedFrame(0x8, nullptr, 0);
                    std::scoped_lock lk(m_send_mutex);
                    sendPayload(frame.data(), frame.size());
                    return Step::Closed;
                }
                case 0x9: {
                    const std::string frame = MaskedFrame(0xA, payload.data(), payload.size());
                    std::scoped_lock lk(m_send_mutex);
                    sendPayload(frame.data(), frame.size());
                    return Step::Handled;
                }
                default:
                    return Step::Handled;
            }
        }

        void enqueue(std::string message)
        {
            {
                std::scoped_lock lk(m_queue_mutex);
                m_incoming.push(std::move(message));
            }
            m_queue_cv.notify_all();
        }

        SOCKET m_socket = INVALID_SOCKET;
        bool m_tls = false;
        bool m_has_cred = false;
        bool m_has_ctx = false;
        CredHandle m_cred{};
        CtxtHandle m_ctx{};
        SecPkgContext_StreamSizes m_sizes{};

        std::string m_rx_raw;
        std::string m_rx_plain;
        std::string m_fragment;

        std::atomic<readyStateValues> m_state{CONNECTING};
        std::atomic<bool> m_running{true};
        std::thread m_reader;

        std::mutex m_send_mutex;

        std::queue<std::string> m_incoming;
        std::mutex m_queue_mutex;
        std::condition_variable m_queue_cv;
    };

}

namespace easywsclient::schannel {

    easywsclient::WebSocket::pointer connect(const std::string& host, int port, bool is_ssl, const std::string& path,
                                             const easywsclient::HeaderKeyValuePair& additional_headers, const std::string& origin)
    {
        std::unique_ptr<_SchannelWebSocket> ws(new _SchannelWebSocket);
        if (!ws->open(host, port, is_ssl, path, additional_headers, origin)) {
            return nullptr;
        }
        return ws.release();
    }

}
