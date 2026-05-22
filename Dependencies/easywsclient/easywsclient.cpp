// easywsclient — WinHTTP/Schannel backend
//
// The public API (see easywsclient.hpp) is preserved so callers don't change.
// Internally we use the WinHTTP WebSocket API (Win8+) which provides TLS via
// Schannel and a fully-fledged client framing implementation.

#pragma warning(disable : 4100)
#pragma warning(disable : 4127)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winhttp.h>

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "easywsclient.hpp"

namespace { // private module-only namespace

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

    bool ParseUrl(const std::string& url, bool& is_ssl, std::string& host,
                  int& port, std::string& path)
    {
        // Accepts ws://host[:port][/path] and wss://host[:port][/path]
        if (url.size() < 6) return false;
        size_t i = 0;
        if (url.compare(0, 6, "wss://") == 0) {
            is_ssl = true;
            i = 6;
        }
        else if (url.compare(0, 5, "ws://") == 0) {
            is_ssl = false;
            i = 5;
        }
        else {
            return false;
        }

        const size_t host_start = i;
        while (i < url.size() && url[i] != ':' && url[i] != '/') ++i;
        host.assign(url, host_start, i - host_start);
        if (host.empty()) return false;

        port = is_ssl ? 443 : 80;
        if (i < url.size() && url[i] == ':') {
            ++i;
            const size_t port_start = i;
            while (i < url.size() && url[i] != '/') ++i;
            if (i == port_start) return false;
            port = std::atoi(url.substr(port_start, i - port_start).c_str());
            if (port <= 0 || port > 65535) return false;
        }
        if (i < url.size() && url[i] == '/') {
            path.assign(url, i, std::string::npos);
        }
        else {
            path = "/";
        }
        return true;
    }

    class _DummyWebSocket : public easywsclient::WebSocket {
    public:
        void poll(int /*timeout*/) override {}
        void send(const std::string& /*message*/) override {}
        void sendPing() override {}
        void close() override {}
        void _dispatch(Callback& /*callable*/) override {}
        readyStateValues getReadyState() const override { return CLOSED; }
    };

    class _RealWebSocket : public easywsclient::WebSocket {
    public:
        _RealWebSocket(HINTERNET hSession, HINTERNET hConnect,
                       HINTERNET hRequest, HINTERNET hWebSocket)
            : m_hSession(hSession)
            , m_hConnect(hConnect)
            , m_hRequest(hRequest)
            , m_hWebSocket(hWebSocket)
            , m_State(OPEN)
            , m_ReaderRunning(true)
            , m_Reader(&_RealWebSocket::ReaderEntry, this)
        {}

        ~_RealWebSocket() override
        {
            m_ReaderRunning = false;

            // Close the WebSocket handle to unblock any pending receive in the
            // reader thread (forces a hard teardown).
            {
                std::lock_guard lk(m_HandleMutex);
                if (m_hWebSocket) {
                    WinHttpCloseHandle(m_hWebSocket);
                    m_hWebSocket = nullptr;
                }
            }
            if (m_Reader.joinable()) {
                m_Reader.join();
            }

            std::lock_guard lk(m_HandleMutex);
            if (m_hRequest) { WinHttpCloseHandle(m_hRequest); m_hRequest = nullptr; }
            if (m_hConnect) { WinHttpCloseHandle(m_hConnect); m_hConnect = nullptr; }
            if (m_hSession) { WinHttpCloseHandle(m_hSession); m_hSession = nullptr; }
        }

        readyStateValues getReadyState() const override
        {
            return m_State.load(std::memory_order_acquire);
        }

        void poll(int timeout) override
        {
            // I/O is driven by the reader thread; poll() simply gives callers a
            // chance to yield. We still honour the timeout for CLOSED so callers
            // that spin on getReadyState() don't busy-loop.
            if (m_State.load() == CLOSED) {
                if (timeout > 0) {
                    Sleep(static_cast<DWORD>(timeout));
                }
                return;
            }
            if (timeout > 0) {
                // Wait up to `timeout` ms for an incoming message to become
                // available, so callers that drain in dispatch() get prompt
                // wakeups.
                const auto deadline = std::chrono::steady_clock::now()
                                      + std::chrono::milliseconds(timeout);
                std::unique_lock lk(m_QueueMutex);
                m_QueueCv.wait_until(lk, deadline, [this] {
                    return !m_IncomingQueue.empty()
                        || m_State.load() == CLOSED;
                });
            }
        }

        void sendPing() override
        {
            // WinHTTP's WebSocket implementation handles PING/PONG frames
            // transparently. Nothing to do.
        }

        void send(const std::string& message) override
        {
            HINTERNET h = nullptr;
            {
                std::lock_guard lk(m_HandleMutex);
                h = m_hWebSocket;
            }
            if (!h) return;
            if (m_State.load() != OPEN) return;
            std::lock_guard sl(m_SendMutex);
            const DWORD result = WinHttpWebSocketSend(
                h,
                WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                message.empty() ? nullptr
                                : const_cast<char*>(message.data()),
                static_cast<DWORD>(message.size()));
            if (result != NO_ERROR) {
                log_error("WinHttpWebSocketSend failed: %lu\n", result);
                m_State.store(CLOSED);
                m_QueueCv.notify_all();
            }
        }

        void close() override
        {
            readyStateValues expected = OPEN;
            if (!m_State.compare_exchange_strong(expected, CLOSING)) {
                return;
            }
            HINTERNET h = nullptr;
            {
                std::lock_guard lk(m_HandleMutex);
                h = m_hWebSocket;
            }
            if (h) {
                std::lock_guard sl(m_SendMutex);
                // Shutdown sends a close frame without blocking on the peer's
                // response. The reader thread will see the peer's close and
                // exit, which flips m_State to CLOSED.
                WinHttpWebSocketShutdown(
                    h, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS,
                    nullptr, 0);
            }
        }

        void _dispatch(Callback& callable) override
        {
            std::queue<std::string> drain;
            {
                std::lock_guard lk(m_QueueMutex);
                drain.swap(m_IncomingQueue);
            }
            while (!drain.empty()) {
                callable(drain.front());
                drain.pop();
            }
        }

    private:
        void ReaderEntry()
        {
            ReaderLoop();
            m_State.store(CLOSED);
            m_QueueCv.notify_all();
        }

        void ReaderLoop()
        {
            std::vector<BYTE> message;
            BYTE buffer[4096];
            while (m_ReaderRunning.load()) {
                HINTERNET h = nullptr;
                {
                    std::lock_guard lk(m_HandleMutex);
                    h = m_hWebSocket;
                }
                if (!h) return;

                DWORD bytes_read = 0;
                WINHTTP_WEB_SOCKET_BUFFER_TYPE buf_type = {};
                const DWORD result = WinHttpWebSocketReceive(
                    h, buffer, sizeof(buffer), &bytes_read, &buf_type);
                if (result != NO_ERROR) {
                    return;
                }
                if (bytes_read > 0) {
                    message.insert(message.end(), buffer, buffer + bytes_read);
                }

                switch (buf_type) {
                    case WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE:
                    case WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE:
                        // Wait for the rest of the message.
                        break;
                    case WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE:
                    case WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE: {
                        std::string msg(reinterpret_cast<const char*>(message.data()),
                                        message.size());
                        message.clear();
                        {
                            std::lock_guard lk(m_QueueMutex);
                            m_IncomingQueue.emplace(std::move(msg));
                        }
                        m_QueueCv.notify_all();
                        break;
                    }
                    case WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE:
                        // Reply to the peer's close frame, then exit.
                        WinHttpWebSocketClose(
                            h, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS,
                            nullptr, 0);
                        return;
                    default:
                        break;
                }
            }
        }

        HINTERNET m_hSession;
        HINTERNET m_hConnect;
        HINTERNET m_hRequest;
        HINTERNET m_hWebSocket;
        std::mutex m_HandleMutex;

        std::atomic<readyStateValues> m_State;
        std::atomic<bool> m_ReaderRunning;
        std::thread m_Reader;

        std::mutex m_SendMutex;

        std::queue<std::string> m_IncomingQueue;
        std::mutex m_QueueMutex;
        std::condition_variable m_QueueCv;
    };

    easywsclient::WebSocket::pointer from_url(
        const std::string& url,
        bool /*useMask*/,
        easywsclient::HeaderKeyValuePair additional_headers,
        const std::string& origin)
    {
        bool is_ssl = false;
        std::string host;
        int port = 0;
        std::string path;
        if (!ParseUrl(url, is_ssl, host, port, path)) {
            log_error("ERROR: Could not parse WebSocket url: %s\n", url.c_str());
            return nullptr;
        }
        log_error("easywsclient: connecting: ssl=%s host=%s port=%d path=%s\n",
                  is_ssl ? "true" : "false", host.c_str(), port, path.c_str());

        const std::wstring whost = Utf8ToWide(host);
        const std::wstring wpath = Utf8ToWide(path);

        HINTERNET hSession = WinHttpOpen(
            L"easywsclient",
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);
        if (!hSession) {
            log_error("WinHttpOpen failed: %lu\n", GetLastError());
            return nullptr;
        }

        WinHttpSetTimeouts(hSession, 10000, 10000, 30000, 30000);

        HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(),
                                             static_cast<INTERNET_PORT>(port), 0);
        if (!hConnect) {
            log_error("WinHttpConnect failed: %lu\n", GetLastError());
            WinHttpCloseHandle(hSession);
            return nullptr;
        }

        DWORD flags = is_ssl ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect, L"GET", wpath.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            log_error("WinHttpOpenRequest failed: %lu\n", GetLastError());
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return nullptr;
        }

        // Preserve the previous behaviour of ignoring TLS errors on the WS
        // upgrade path (the old code routed all verify callbacks to success).
        if (is_ssl) {
            DWORD sec_flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA
                            | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID
                            | SECURITY_FLAG_IGNORE_CERT_CN_INVALID
                            | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS,
                              &sec_flags, sizeof(sec_flags));
        }

        // Request a WebSocket upgrade on this request.
        if (!WinHttpSetOption(hRequest, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,
                               nullptr, 0)) {
            log_error("WinHttpSetOption(UPGRADE_TO_WEB_SOCKET) failed: %lu\n",
                      GetLastError());
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return nullptr;
        }

        // Build extra headers (Origin + caller-supplied).
        std::wstring extra_headers;
        if (!origin.empty()) {
            extra_headers.append(L"Origin: ");
            extra_headers.append(Utf8ToWide(origin));
            extra_headers.append(L"\r\n");
        }
        for (const auto& kv : additional_headers) {
            extra_headers.append(Utf8ToWide(kv.first));
            extra_headers.append(L": ");
            extra_headers.append(Utf8ToWide(kv.second));
            extra_headers.append(L"\r\n");
        }

        const wchar_t* headers = extra_headers.empty()
                                     ? WINHTTP_NO_ADDITIONAL_HEADERS
                                     : extra_headers.c_str();
        const DWORD headers_len = extra_headers.empty()
                                      ? 0
                                      : static_cast<DWORD>(extra_headers.size());

        if (!WinHttpSendRequest(hRequest, headers, headers_len,
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            log_error("WinHttpSendRequest failed: %lu\n", GetLastError());
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return nullptr;
        }

        if (!WinHttpReceiveResponse(hRequest, nullptr)) {
            log_error("WinHttpReceiveResponse failed: %lu\n", GetLastError());
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return nullptr;
        }

        DWORD status_code = 0;
        DWORD size = sizeof(status_code);
        WinHttpQueryHeaders(hRequest,
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX,
                             &status_code, &size, WINHTTP_NO_HEADER_INDEX);
        if (status_code != HTTP_STATUS_SWITCH_PROTOCOLS) {
            log_error("ERROR: Got bad status connecting to %s: %lu\n",
                      url.c_str(), status_code);
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return nullptr;
        }

        HINTERNET hWebSocket = WinHttpWebSocketCompleteUpgrade(hRequest, 0);
        if (!hWebSocket) {
            log_error("WinHttpWebSocketCompleteUpgrade failed: %lu\n",
                      GetLastError());
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return nullptr;
        }

        log_error("Connected to: %s\n", url.c_str());
        return new _RealWebSocket(hSession, hConnect, hRequest, hWebSocket);
    }

} // namespace


namespace easywsclient {

    WebSocket::pointer WebSocket::create_dummy()
    {
        static pointer dummy = pointer(new _DummyWebSocket);
        return dummy;
    }

    WebSocket::pointer WebSocket::from_url(const std::string& url,
                                            const HeaderKeyValuePair& additional_headers,
                                            const std::string& origin)
    {
        return ::from_url(url, true, additional_headers, origin);
    }

    WebSocket::pointer WebSocket::from_url_no_mask(const std::string& url,
                                                    const HeaderKeyValuePair& additional_headers,
                                                    const std::string& origin)
    {
        // Mask handling is internal to the WinHTTP WebSocket implementation;
        // we accept the parameter for API compatibility.
        return ::from_url(url, false, additional_headers, origin);
    }

} // namespace easywsclient
