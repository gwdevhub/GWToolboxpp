#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#define SECURITY_WIN32
#include <schannel.h>
#include <security.h>

#include "SchannelTls.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace {

    bool EnsureWinsock()
    {
        static const bool started = [] {
            WSADATA data{};
            return WSAStartup(MAKEWORD(2, 2), &data) == 0;
        }();
        return started;
    }

    std::wstring Utf8ToWide(const std::string& s)
    {
        const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
        if (len <= 0) return {};
        std::wstring out(static_cast<size_t>(len), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), len);
        return out;
    }

    uint32_t SocketError()
    {
        const int err = WSAGetLastError();
        return err == WSAETIMEDOUT ? static_cast<uint32_t>(ERROR_WINHTTP_TIMEOUT) : static_cast<uint32_t>(err);
    }

} // namespace

struct SchannelTlsStream::Impl {
    SOCKET m_socket = INVALID_SOCKET;
    bool m_encrypted = false;
    bool m_eof = false;
    bool m_has_cred = false;
    bool m_has_ctx = false;
    CredHandle m_cred{};
    CtxtHandle m_ctx{};
    SecPkgContext_StreamSizes m_sizes{};
    std::string m_rx_raw;
    std::string m_rx_plain;
    uint32_t m_last_error = 0;

    ~Impl()
    {
        if (m_socket != INVALID_SOCKET) closesocket(m_socket);
        if (m_has_ctx) DeleteSecurityContext(&m_ctx);
        if (m_has_cred) FreeCredentialsHandle(&m_cred);
    }

    bool ConnectWithTimeout(SOCKET s, const addrinfo* address, int timeout_ms)
    {
        u_long nonblocking = 1;
        ioctlsocket(s, FIONBIO, &nonblocking);
        if (connect(s, address->ai_addr, static_cast<int>(address->ai_addrlen)) == SOCKET_ERROR) {
            if (WSAGetLastError() != WSAEWOULDBLOCK) {
                m_last_error = SocketError();
                return false;
            }
            fd_set writable;
            FD_ZERO(&writable);
            FD_SET(s, &writable);
            fd_set failed;
            FD_ZERO(&failed);
            FD_SET(s, &failed);
            timeval wait{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
            const int ready = select(0, nullptr, &writable, &failed, &wait);
            if (ready == 0) {
                m_last_error = ERROR_WINHTTP_TIMEOUT;
                return false;
            }
            if (ready == SOCKET_ERROR) {
                m_last_error = SocketError();
                return false;
            }
            int so_error = 0;
            int so_error_len = sizeof(so_error);
            getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_error), &so_error_len);
            if (so_error != 0) {
                m_last_error = static_cast<uint32_t>(so_error);
                return false;
            }
        }
        u_long blocking = 0;
        ioctlsocket(s, FIONBIO, &blocking);
        return true;
    }

    bool ConnectSocket(const std::string& host, uint16_t port, int connect_timeout_ms)
    {
        if (!EnsureWinsock()) {
            m_last_error = SocketError();
            return false;
        }
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* list = nullptr;
        const std::string service = std::to_string(port);
        const int lookup = getaddrinfo(host.c_str(), service.c_str(), &hints, &list);
        if (lookup != 0) {
            m_last_error = static_cast<uint32_t>(lookup);
            return false;
        }
        for (addrinfo* address = list; address && m_socket == INVALID_SOCKET; address = address->ai_next) {
            const SOCKET s = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
            if (s == INVALID_SOCKET) {
                m_last_error = SocketError();
                continue;
            }
            if (ConnectWithTimeout(s, address, connect_timeout_ms)) {
                m_socket = s;
            }
            else {
                closesocket(s);
            }
        }
        freeaddrinfo(list);
        return m_socket != INVALID_SOCKET;
    }

    bool ReceiveRaw(std::string& buf)
    {
        char chunk[16384];
        const int n = recv(m_socket, chunk, sizeof(chunk), 0);
        if (n == 0) {
            m_last_error = WSAECONNRESET;
            return false;
        }
        if (n < 0) {
            m_last_error = SocketError();
            return false;
        }
        buf.append(chunk, static_cast<size_t>(n));
        return true;
    }

    bool SendRaw(const char* data, size_t len)
    {
        while (len) {
            const int n = send(m_socket, data, static_cast<int>(len), 0);
            if (n <= 0) {
                m_last_error = SocketError();
                return false;
            }
            data += n;
            len -= static_cast<size_t>(n);
        }
        return true;
    }

    bool Handshake(const std::string& host)
    {
        SCHANNEL_CRED cred_data{};
        cred_data.dwVersion = SCHANNEL_CRED_VERSION;
        cred_data.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT;
        cred_data.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS;
        TimeStamp expiry{};
        const SECURITY_STATUS acquired = AcquireCredentialsHandleW(
            nullptr, const_cast<SEC_WCHAR*>(L"Microsoft Unified Security Protocol Provider"),
            SECPKG_CRED_OUTBOUND, nullptr, &cred_data, nullptr, nullptr, &m_cred, &expiry);
        if (acquired != SEC_E_OK) {
            m_last_error = static_cast<uint32_t>(acquired);
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
                const bool sent = SendRaw(static_cast<const char*>(out_buf.pvBuffer), out_buf.cbBuffer);
                FreeContextBuffer(out_buf.pvBuffer);
                if (!sent) {
                    return false;
                }
            }
            if (st == SEC_E_INCOMPLETE_MESSAGE) {
                if (!ReceiveRaw(m_rx_raw)) {
                    return false;
                }
                continue;
            }
            if (st != SEC_E_OK && st != SEC_I_CONTINUE_NEEDED) {
                m_last_error = static_cast<uint32_t>(st);
                return false;
            }
            const size_t extra = in_bufs[1].BufferType == SECBUFFER_EXTRA ? in_bufs[1].cbBuffer : 0;
            m_rx_raw.erase(0, m_rx_raw.size() - extra);
            if (st == SEC_E_OK) {
                break;
            }
            if (extra == 0 && !ReceiveRaw(m_rx_raw)) {
                return false;
            }
        }

        if (QueryContextAttributesW(&m_ctx, SECPKG_ATTR_STREAM_SIZES, &m_sizes) != SEC_E_OK) {
            m_last_error = ERROR_WINHTTP_SECURE_FAILURE;
            return false;
        }
        m_encrypted = true;
        return m_rx_raw.empty() || DecryptPending();
    }

    bool DecryptPending()
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
            if (st == SEC_I_CONTEXT_EXPIRED) {
                m_eof = true;
                m_rx_raw.clear();
                return true;
            }
            if (st != SEC_E_OK) {
                m_last_error = static_cast<uint32_t>(st);
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

    bool SendEncrypted(const char* data, size_t len)
    {
        size_t offset = 0;
        while (offset < len) {
            const size_t chunk = std::min<size_t>(len - offset, m_sizes.cbMaximumMessage);
            std::string buf(m_sizes.cbHeader + chunk + m_sizes.cbTrailer, '\0');
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
            if (const SECURITY_STATUS st = EncryptMessage(&m_ctx, 0, &desc, 0); st != SEC_E_OK) {
                m_last_error = static_cast<uint32_t>(st);
                return false;
            }
            const size_t total = bufs[0].cbBuffer + bufs[1].cbBuffer + bufs[2].cbBuffer;
            if (!SendRaw(buf.data(), total)) {
                return false;
            }
            offset += chunk;
        }
        return true;
    }

    int ReceivePlain(char* buf, size_t len)
    {
        const int n = recv(m_socket, buf, static_cast<int>(len), 0);
        if (n < 0) {
            m_last_error = SocketError();
        }
        return n;
    }

    int ReceiveDecrypted(char* buf, size_t len)
    {
        for (;;) {
            if (!m_rx_plain.empty()) {
                const size_t n = std::min(len, m_rx_plain.size());
                memcpy(buf, m_rx_plain.data(), n);
                m_rx_plain.erase(0, n);
                return static_cast<int>(n);
            }
            if (m_eof) {
                return 0;
            }
            if (!m_rx_raw.empty() && !DecryptPending()) {
                return -1;
            }
            if (!m_rx_plain.empty() || m_eof) {
                continue;
            }
            char chunk[16384];
            const int n = recv(m_socket, chunk, sizeof(chunk), 0);
            if (n < 0) {
                m_last_error = SocketError();
                return -1;
            }
            if (n == 0) {
                m_eof = true;
                continue;
            }
            m_rx_raw.append(chunk, static_cast<size_t>(n));
        }
    }
};

SchannelTlsStream::SchannelTlsStream()
    : m_impl(std::make_unique<Impl>())
{
}

SchannelTlsStream::~SchannelTlsStream() = default;

bool SchannelTlsStream::Connect(const std::string& host, const uint16_t port, const bool use_tls, const int connect_timeout_ms, const int rw_timeout_ms)
{
    if (!m_impl->ConnectSocket(host, port, connect_timeout_ms)) {
        return false;
    }
    const DWORD rw_timeout = static_cast<DWORD>(rw_timeout_ms);
    setsockopt(m_impl->m_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&rw_timeout), sizeof(rw_timeout));
    setsockopt(m_impl->m_socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&rw_timeout), sizeof(rw_timeout));
    return !use_tls || m_impl->Handshake(host);
}

bool SchannelTlsStream::Send(const char* data, const size_t len)
{
    return m_impl->m_encrypted ? m_impl->SendEncrypted(data, len) : m_impl->SendRaw(data, len);
}

int SchannelTlsStream::Receive(char* buf, const size_t len)
{
    return m_impl->m_encrypted ? m_impl->ReceiveDecrypted(buf, len) : m_impl->ReceivePlain(buf, len);
}

void SchannelTlsStream::Shutdown()
{
    if (m_impl->m_socket != INVALID_SOCKET) {
        shutdown(m_impl->m_socket, SD_BOTH);
    }
}

uint32_t SchannelTlsStream::LastError() const
{
    return m_impl->m_last_error;
}
