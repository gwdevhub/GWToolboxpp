#include "stdafx.h"

#include "HttpClient.h"
#include "SchannelTls.h"

#include <windows.h>
#include <winhttp.h>

#include <cstdlib>
#include <cstring>
#include <cwchar>

namespace {

    std::atomic<int> g_InitializeCount;

    std::wstring Utf8ToWide(const char* s, const size_t count = static_cast<size_t>(-1))
    {
        if (!s) return {};
        const int len_in = std::cmp_equal(count, -1) ? -1 : static_cast<int>(count);
        const int wlen = MultiByteToWideChar(CP_UTF8, 0, s, len_in, nullptr, 0);
        if (wlen <= 0) return {};
        std::wstring out;
        out.resize(static_cast<size_t>(len_in == -1 ? wlen - 1 : wlen));
        MultiByteToWideChar(CP_UTF8, 0, s, len_in, out.data(), wlen);
        return out;
    }

    std::string WideToUtf8(const wchar_t* s, const size_t count)
    {
        if (!s || count == 0) return {};
        const int len_in = static_cast<int>(count);
        const int u8len = WideCharToMultiByte(CP_UTF8, 0, s, len_in, nullptr, 0, nullptr, nullptr);
        if (u8len <= 0) return {};
        std::string out;
        out.resize(static_cast<size_t>(u8len));
        WideCharToMultiByte(CP_UTF8, 0, s, len_in, out.data(), u8len, nullptr, nullptr);
        return out;
    }

    bool StartsWith(const char* str, const char* prefix)
    {
        for (; *prefix != 0; ++prefix, ++str) {
            if (*prefix != *str) {
                return false;
            }
        }
        return true;
    }

    bool EndsWith(const std::string& str, const char* suffix)
    {
        const size_t suffix_len = strlen(suffix);
        if (str.size() < suffix_len) {
            return false;
        }
        const size_t str_start = str.size() - suffix_len;
        for (size_t i = 0; i < suffix_len; ++i) {
            if (str[str_start + i] != suffix[i]) {
                return false;
            }
        }
        return true;
    }

    struct ParsedUrl {
        bool tls = false;
        std::string host;
        uint16_t port = 0;
        std::string host_header;
        std::string target;
    };

    bool ParseUrl(const std::string& url, ParsedUrl& out)
    {
        const std::wstring wide = Utf8ToWide(url.c_str(), url.size());
        URL_COMPONENTSW comp = {};
        comp.dwStructSize = sizeof(comp);
        wchar_t scheme_buf[16] = {};
        wchar_t host_buf[256] = {};
        wchar_t path_buf[2048] = {};
        wchar_t extra_buf[2048] = {};
        comp.lpszScheme = scheme_buf;
        comp.dwSchemeLength = _countof(scheme_buf);
        comp.lpszHostName = host_buf;
        comp.dwHostNameLength = _countof(host_buf);
        comp.lpszUrlPath = path_buf;
        comp.dwUrlPathLength = _countof(path_buf);
        comp.lpszExtraInfo = extra_buf;
        comp.dwExtraInfoLength = _countof(extra_buf);
        if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &comp)) {
            return false;
        }
        if (comp.nScheme != INTERNET_SCHEME_HTTP && comp.nScheme != INTERNET_SCHEME_HTTPS) {
            return false;
        }
        out.tls = comp.nScheme == INTERNET_SCHEME_HTTPS;
        out.host = WideToUtf8(host_buf, comp.dwHostNameLength);
        out.port = comp.nPort;
        const bool default_port = (out.tls && out.port == 443) || (!out.tls && out.port == 80);
        out.host_header = default_port ? out.host : out.host + ":" + std::to_string(out.port);
        out.target = WideToUtf8(path_buf, comp.dwUrlPathLength) + WideToUtf8(extra_buf, comp.dwExtraInfoLength);
        if (out.target.empty()) {
            out.target = "/";
        }
        return true;
    }

    std::string ResolveRedirect(const ParsedUrl& base, const std::string& location)
    {
        if (StartsWith(location.c_str(), "http://") || StartsWith(location.c_str(), "https://")) {
            return location;
        }
        const std::string origin = std::string(base.tls ? "https://" : "http://") + base.host_header;
        if (!location.empty() && location[0] == '/') {
            return origin + location;
        }
        const size_t last_slash = base.target.find_last_of('/');
        return origin + base.target.substr(0, last_slash + 1) + location;
    }

    std::string FindHeader(const std::string& head, const char* name)
    {
        const size_t name_len = strlen(name);
        size_t line_start = head.find("\r\n");
        while (line_start != std::string::npos) {
            line_start += 2;
            const size_t line_end = head.find("\r\n", line_start);
            const std::string line = head.substr(line_start, line_end == std::string::npos ? std::string::npos : line_end - line_start);
            if (line.size() > name_len && _strnicmp(line.c_str(), name, name_len) == 0 && line[name_len] == ':') {
                size_t value_start = name_len + 1;
                while (value_start < line.size() && line[value_start] == ' ') {
                    ++value_start;
                }
                return line.substr(value_start);
            }
            line_start = line_end;
        }
        return {};
    }

    int ParseStatusCode(const std::string& head)
    {
        if (!StartsWith(head.c_str(), "HTTP/")) {
            return 0;
        }
        const size_t space = head.find(' ');
        if (space == std::string::npos) {
            return 0;
        }
        return atoi(head.c_str() + space + 1);
    }

    bool ReadHead(SchannelTlsStream& stream, std::string& head, std::string& leftover)
    {
        constexpr size_t max_head_size = 64 * 1024;
        std::string buf;
        for (;;) {
            const size_t end = buf.find("\r\n\r\n");
            if (end != std::string::npos) {
                std::string candidate = buf.substr(0, end + 4);
                if (ParseStatusCode(candidate) / 100 == 1) {
                    buf.erase(0, end + 4);
                    continue;
                }
                head = std::move(candidate);
                leftover = buf.substr(end + 4);
                return true;
            }
            if (buf.size() > max_head_size) {
                return false;
            }
            char chunk[4096];
            const int n = stream.Receive(chunk, sizeof(chunk));
            if (n <= 0) {
                return false;
            }
            buf.append(chunk, static_cast<size_t>(n));
        }
    }

    class HttpBodyReader {
    public:
        enum class Framing { UntilClose, Length, Chunked };

        HttpBodyReader(SchannelTlsStream& stream, std::string buffered, const Framing framing, const uint64_t length)
            : m_stream(stream)
            , m_buffered(std::move(buffered))
            , m_framing(framing)
            , m_remaining(length)
        {
        }

        int Read(char* out, const size_t cap)
        {
            switch (m_framing) {
                case Framing::UntilClose: return ReadRaw(out, cap);
                case Framing::Length:     return ReadLength(out, cap);
                case Framing::Chunked:    return ReadChunk(out, cap);
            }
            return -1;
        }

    private:
        int ReadRaw(char* out, const size_t cap)
        {
            if (!m_buffered.empty()) {
                const size_t n = std::min(cap, m_buffered.size());
                memcpy(out, m_buffered.data(), n);
                m_buffered.erase(0, n);
                return static_cast<int>(n);
            }
            return m_stream.Receive(out, cap);
        }

        int ReadLength(char* out, const size_t cap)
        {
            if (m_remaining == 0) {
                return 0;
            }
            const int n = ReadRaw(out, static_cast<size_t>(std::min<uint64_t>(cap, m_remaining)));
            if (n == 0) {
                return -1;
            }
            if (n > 0) {
                m_remaining -= static_cast<uint64_t>(n);
            }
            return n;
        }

        int ReadChunk(char* out, const size_t cap)
        {
            if (m_chunk_remaining == 0) {
                if (m_chunk_started) {
                    std::string crlf;
                    if (!ReadLine(crlf)) {
                        return -1;
                    }
                }
                std::string size_line;
                if (!ReadLine(size_line)) {
                    return -1;
                }
                m_chunk_started = true;
                m_chunk_remaining = strtoull(size_line.c_str(), nullptr, 16);
                if (m_chunk_remaining == 0) {
                    std::string trailer;
                    do {
                        if (!ReadLine(trailer)) {
                            return -1;
                        }
                    } while (!trailer.empty());
                    return 0;
                }
            }
            const int n = ReadRaw(out, static_cast<size_t>(std::min<uint64_t>(cap, m_chunk_remaining)));
            if (n <= 0) {
                return -1;
            }
            m_chunk_remaining -= static_cast<uint64_t>(n);
            return n;
        }

        bool ReadLine(std::string& line)
        {
            for (;;) {
                const size_t end = m_buffered.find("\r\n");
                if (end != std::string::npos) {
                    line = m_buffered.substr(0, end);
                    m_buffered.erase(0, end + 2);
                    return true;
                }
                char chunk[4096];
                const int n = m_stream.Receive(chunk, sizeof(chunk));
                if (n <= 0) {
                    return false;
                }
                m_buffered.append(chunk, static_cast<size_t>(n));
            }
        }

        SchannelTlsStream& m_stream;
        std::string m_buffered;
        Framing m_framing;
        uint64_t m_remaining = 0;
        uint64_t m_chunk_remaining = 0;
        bool m_chunk_started = false;
    };

} // namespace

void InitHttpClient()
{
    ++g_InitializeCount;
}

void ShutdownHttpClient()
{
    assert(g_InitializeCount > 0);
    --g_InitializeCount;
}

HttpRequest::HttpRequest()
    : m_Status(ResponseStatus::None)
    , m_StatusCode(0)
    , m_PostBody(nullptr)
    , m_PostBodySize(0)
    , m_TimeoutMs(0)
    , m_ConnectTimeoutMs(0)
    , m_MaxRedirects(-1)
    , m_FollowLocation(false)
    , m_VerifyPeer(true)
    , m_VerifyHost(true)
    , m_NoBody(false)
    , m_HasCustomMethod(false)
    , m_hSession(nullptr)
    , m_hConnect(nullptr)
    , m_hRequest(nullptr)
    , m_CancelMutex(nullptr)
{
    m_Aborted.store(false);
    auto* cs = new CRITICAL_SECTION();
    InitializeCriticalSection(cs);
    m_CancelMutex = cs;
}

HttpRequest::~HttpRequest()
{
    CloseHandles();
    if (m_CancelMutex) {
        auto* cs = static_cast<CRITICAL_SECTION*>(m_CancelMutex);
        DeleteCriticalSection(cs);
        delete cs;
        m_CancelMutex = nullptr;
    }
}

void HttpRequest::SetUrl(const char* url)
{
    m_Url = Utf8ToWide(url);
}

void HttpRequest::SetUrl(const Protocol proto, const char* url)
{
    const char* prefix = GetProtocolPrefix(proto);
    if (StartsWith(url, prefix)) {
        SetUrl(url);
        return;
    }
    std::string temp;
    temp.append(prefix);
    temp.append(url);
    SetUrl(temp.c_str());
}

void HttpRequest::SetMethod(const char* method)
{
    if (method && *method) {
        m_Method = Utf8ToWide(method);
        m_HasCustomMethod = true;
    }
}

void HttpRequest::SetMethod(const HttpMethod method)
{
    m_Method = Utf8ToWide(GetHttpMethodName(method));
    m_HasCustomMethod = true;
}

void HttpRequest::SetHeader(const char* field)
{
    if (!field) return;
    if (!m_HeadersBlock.empty()) {
        m_HeadersBlock.append(L"\r\n");
    }
    m_HeadersBlock.append(Utf8ToWide(field));
}

void HttpRequest::SetHeader(const char* name, const char* value)
{
    if (!name || !value) return;
    if (!m_HeadersBlock.empty()) {
        m_HeadersBlock.append(L"\r\n");
    }
    m_HeadersBlock.append(Utf8ToWide(name));
    m_HeadersBlock.append(L": ");
    m_HeadersBlock.append(Utf8ToWide(value));
}

void HttpRequest::SetHeaders(const std::initializer_list<ParamField> headers)
{
    for (const ParamField& h : headers) {
        SetHeader(h.first, h.second);
    }
}

void HttpRequest::SetUserAgent(const char* user_agent)
{
    m_UserAgent = Utf8ToWide(user_agent);
}

void HttpRequest::SetTimeoutMs(const int timeout_ms)
{
    m_TimeoutMs = timeout_ms;
}

void HttpRequest::SetTimeoutSec(const int timeout_sec)
{
    m_TimeoutMs = timeout_sec * 1000;
}

void HttpRequest::SetConnectTimeoutMs(const int timeout_ms)
{
    m_ConnectTimeoutMs = timeout_ms;
}

void HttpRequest::SetConnectTimeoutSec(const int timeout_sec)
{
    m_ConnectTimeoutMs = timeout_sec * 1000;
}

void HttpRequest::SetMaxRedirects(const int amount)
{
    m_MaxRedirects = amount;
}

void HttpRequest::SetNoBody(const bool enable)
{
    m_NoBody = enable;
}

void HttpRequest::SetVerifyPeer(const bool enable)
{
    m_VerifyPeer = enable;
}

void HttpRequest::SetVerifyHost(const bool enable)
{
    m_VerifyHost = enable;
}

void HttpRequest::SetFollowLocation(const bool enable)
{
    m_FollowLocation = enable;
}

void HttpRequest::SetProxy(const char* url)
{
    m_Proxy = Utf8ToWide(url);
}

void HttpRequest::SetPostContent(const std::string& content, const ContentFlag flag)
{
    SetPostContent(content.c_str(), content.size(), flag);
}

void HttpRequest::SetPostContent(const char* content, const size_t size, const ContentFlag flag)
{
    assert(content != nullptr);
    if (!m_HasCustomMethod) {
        SetMethod(HttpMethod::Post);
    }
    if (flag == ContentFlag::Copy) {
        m_PostBodyStorage.assign(content, size);
        m_PostBody = m_PostBodyStorage.data();
    }
    else {
        m_PostBodyStorage.clear();
        m_PostBody = content;
    }
    m_PostBodySize = size;
}

void HttpRequest::Clear()
{
    m_Header.clear();
    m_Content.clear();
    m_Status = ResponseStatus::None;
    m_StatusCode = 0;
    m_TransportError = 0;
    m_Aborted.store(false);
}

void HttpRequest::Reset()
{
    Clear();
    m_Url.clear();
    m_UserAgent.clear();
    m_Method.clear();
    m_HeadersBlock.clear();
    m_Proxy.clear();
    m_PostBodyStorage.clear();
    m_PostBody = nullptr;
    m_PostBodySize = 0;
    m_TimeoutMs = 0;
    m_ConnectTimeoutMs = 0;
    m_MaxRedirects = -1;
    m_FollowLocation = false;
    m_VerifyPeer = true;
    m_VerifyHost = true;
    m_NoBody = false;
    m_HasCustomMethod = false;
}

void HttpRequest::CloseHandles()
{
    HINTERNET request = nullptr;
    HINTERNET connect = nullptr;
    HINTERNET session = nullptr;
    {
        auto* cs = static_cast<CRITICAL_SECTION*>(m_CancelMutex);
        EnterCriticalSection(cs);
        request = static_cast<HINTERNET>(m_hRequest);
        connect = static_cast<HINTERNET>(m_hConnect);
        session = static_cast<HINTERNET>(m_hSession);
        m_hRequest = nullptr;
        m_hConnect = nullptr;
        m_hSession = nullptr;
        LeaveCriticalSection(cs);
    }
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);
}

void HttpRequest::RequestAbort()
{
    HINTERNET to_close = nullptr;
    {
        auto* cs = static_cast<CRITICAL_SECTION*>(m_CancelMutex);
        EnterCriticalSection(cs);
        m_Aborted.store(true);
        to_close = static_cast<HINTERNET>(m_hRequest);
        m_hRequest = nullptr;
        if (m_Tls) {
            m_Tls->Shutdown();
        }
        LeaveCriticalSection(cs);
    }
    // Closing the request handle from another thread cancels any pending
    // sync WinHTTP operation on it.
    if (to_close) {
        WinHttpCloseHandle(to_close);
    }
}

bool HttpRequest::Perform()
{
    Clear();

    if (!m_VerifyPeer && m_Url.size() >= 8 && _wcsnicmp(m_Url.c_str(), L"https://", 8) == 0) {
        return PerformSchannel();
    }

    URL_COMPONENTSW comp = {};
    comp.dwStructSize = sizeof(comp);
    wchar_t scheme_buf[16] = {};
    wchar_t host_buf[256] = {};
    wchar_t path_buf[2048] = {};
    wchar_t extra_buf[2048] = {};
    comp.lpszScheme = scheme_buf;
    comp.dwSchemeLength = _countof(scheme_buf);
    comp.lpszHostName = host_buf;
    comp.dwHostNameLength = _countof(host_buf);
    comp.lpszUrlPath = path_buf;
    comp.dwUrlPathLength = _countof(path_buf);
    comp.lpszExtraInfo = extra_buf;
    comp.dwExtraInfoLength = _countof(extra_buf);

    if (!WinHttpCrackUrl(m_Url.c_str(), 0, 0, &comp)) {
        m_TransportError = GetLastError();
        m_Status = ResponseStatus::Error;
        return false;
    }

    const wchar_t* agent = m_UserAgent.empty() ? L"GWToolbox" : m_UserAgent.c_str();

    HINTERNET hSession = WinHttpOpen(
        agent,
        m_Proxy.empty() ? WINHTTP_ACCESS_TYPE_NO_PROXY : WINHTTP_ACCESS_TYPE_NAMED_PROXY,
        m_Proxy.empty() ? WINHTTP_NO_PROXY_NAME : m_Proxy.c_str(),
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) {
        m_TransportError = GetLastError();
        m_Status = ResponseStatus::Error;
        return false;
    }

    const int conn_timeout = m_ConnectTimeoutMs > 0 ? m_ConnectTimeoutMs : 60000;
    const int rw_timeout = m_TimeoutMs > 0 ? m_TimeoutMs : 30000;
    WinHttpSetTimeouts(hSession, conn_timeout, conn_timeout, rw_timeout, rw_timeout);

    HINTERNET hConnect = WinHttpConnect(hSession, host_buf, comp.nPort, 0);
    if (!hConnect) {
        m_TransportError = GetLastError();
        WinHttpCloseHandle(hSession);
        m_Status = ResponseStatus::Error;
        return false;
    }

    std::wstring full_path = path_buf;
    if (comp.dwExtraInfoLength) {
        full_path.append(extra_buf, comp.dwExtraInfoLength);
    }

    DWORD flags = (comp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    const wchar_t* verb = nullptr;
    if (m_HasCustomMethod && !m_Method.empty()) {
        verb = m_Method.c_str();
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        verb,
        full_path.empty() ? L"/" : full_path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags);
    if (!hRequest) {
        m_TransportError = GetLastError();
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        m_Status = ResponseStatus::Error;
        return false;
    }

    // Publish the handles so RequestAbort() can find them.
    {
        auto* cs = static_cast<CRITICAL_SECTION*>(m_CancelMutex);
        EnterCriticalSection(cs);
        if (m_Aborted.load()) {
            LeaveCriticalSection(cs);
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            m_Status = ResponseStatus::Aborted;
            return false;
        }
        m_hSession = hSession;
        m_hConnect = hConnect;
        m_hRequest = hRequest;
        LeaveCriticalSection(cs);
    }

    // WinHTTP/Schannel verifies certs by default; relax only where the caller opted out.
    if (!m_VerifyPeer || !m_VerifyHost) {
        DWORD secFlags = 0;
        if (!m_VerifyPeer) {
            secFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA;
            secFlags |= SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
            secFlags |= SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        }
        if (!m_VerifyHost) {
            secFlags |= SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        }
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS,
                         &secFlags, sizeof(secFlags));
    }

    {
        DWORD policy = m_FollowLocation
                           ? WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS
                           : WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY,
                         &policy, sizeof(policy));
    }
    if (m_MaxRedirects >= 0) {
        DWORD limit = static_cast<DWORD>(m_MaxRedirects);
        WinHttpSetOption(hRequest, WINHTTP_OPTION_MAX_HTTP_AUTOMATIC_REDIRECTS,
                         &limit, sizeof(limit));
    }

    if (!m_HeadersBlock.empty()) {
        WinHttpAddRequestHeaders(
            hRequest,
            m_HeadersBlock.c_str(),
            static_cast<DWORD>(m_HeadersBlock.size()),
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    LPVOID body_ptr = WINHTTP_NO_REQUEST_DATA;
    DWORD body_len = 0;
    if (m_PostBody && m_PostBodySize) {
        body_ptr = const_cast<void*>(m_PostBody);
        body_len = static_cast<DWORD>(m_PostBodySize);
    }

    BOOL ok = WinHttpSendRequest(
        hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        body_ptr, body_len, body_len,
        0);

    if (ok) {
        ok = WinHttpReceiveResponse(hRequest, nullptr);
    }

    if (!ok) {
        const DWORD err = GetLastError();
        m_TransportError = err;
        CloseHandles();
        if (m_Aborted.load()) {
            m_Status = ResponseStatus::Aborted;
        }
        else if (err == ERROR_WINHTTP_TIMEOUT) {
            m_Status = ResponseStatus::TimedOut;
        }
        else {
            m_Status = ResponseStatus::Error;
        }
        return false;
    }

    {
        DWORD status_code = 0;
        DWORD size = sizeof(status_code);
        WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status_code, &size, WINHTTP_NO_HEADER_INDEX);
        m_StatusCode = static_cast<int>(status_code);
    }

    {
        DWORD bytes = 0;
        WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_RAW_HEADERS_CRLF,
            WINHTTP_HEADER_NAME_BY_INDEX,
            WINHTTP_NO_OUTPUT_BUFFER, &bytes,
            WINHTTP_NO_HEADER_INDEX);
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && bytes > 0) {
            std::wstring raw;
            raw.resize(bytes / sizeof(wchar_t));
            if (WinHttpQueryHeaders(
                    hRequest,
                    WINHTTP_QUERY_RAW_HEADERS_CRLF,
                    WINHTTP_HEADER_NAME_BY_INDEX,
                    raw.data(), &bytes,
                    WINHTTP_NO_HEADER_INDEX)) {
                while (!raw.empty() && raw.back() == L'\0') {
                    raw.pop_back();
                }
                const std::string headers_utf8 = WideToUtf8(raw.data(), raw.size());
                OnHeader(headers_utf8.c_str(), headers_utf8.size());
            }
        }
    }

    if (!m_NoBody) {
        char buf[8192];
        for (;;) {
            DWORD read = 0;
            if (!WinHttpReadData(hRequest, buf, sizeof(buf), &read)) {
                const bool aborted = m_Aborted.load();
                m_TransportError = GetLastError();
                CloseHandles();
                m_Status = aborted ? ResponseStatus::Aborted : ResponseStatus::Error;
                return false;
            }
            if (read == 0) {
                break; // End of body
            }
            OnContent(buf, read);
        }
    }

    CloseHandles();

    if (m_Aborted.load()) {
        m_Status = ResponseStatus::Aborted;
        OnPerformed();
        return false;
    }

    m_Status = ResponseStatus::Completed;
    OnPerformed();
    return true;
}

bool HttpRequest::PublishTls(SchannelTlsStream* stream)
{
    auto* cs = static_cast<CRITICAL_SECTION*>(m_CancelMutex);
    EnterCriticalSection(cs);
    const bool allowed = stream == nullptr || !m_Aborted.load();
    if (allowed) {
        m_Tls = stream;
    }
    LeaveCriticalSection(cs);
    return allowed;
}

bool HttpRequest::PerformSchannel()
{
    std::string url = WideToUtf8(m_Url.c_str(), m_Url.size());
    std::string method = (m_HasCustomMethod && !m_Method.empty())
                             ? WideToUtf8(m_Method.c_str(), m_Method.size())
                             : std::string(m_PostBody ? "POST" : "GET");
    std::string body = m_PostBody ? std::string(static_cast<const char*>(m_PostBody), m_PostBodySize) : std::string{};
    const std::string headers = WideToUtf8(m_HeadersBlock.c_str(), m_HeadersBlock.size());
    const std::string user_agent = m_UserAgent.empty() ? std::string("GWToolbox") : WideToUtf8(m_UserAgent.c_str(), m_UserAgent.size());
    const int conn_timeout = m_ConnectTimeoutMs > 0 ? m_ConnectTimeoutMs : 60000;
    const int rw_timeout = m_TimeoutMs > 0 ? m_TimeoutMs : 30000;
    const int max_redirects = m_FollowLocation ? (m_MaxRedirects >= 0 ? m_MaxRedirects : 10) : 0;

    auto fail = [this](const uint32_t error) {
        m_TransportError = error ? error : ERROR_WINHTTP_CONNECTION_ERROR;
        m_Status = m_Aborted.load() ? ResponseStatus::Aborted : ResponseStatus::Error;
        return false;
    };

    for (int redirects = 0;; ++redirects) {
        ParsedUrl target;
        if (!ParseUrl(url, target)) {
            return fail(ERROR_WINHTTP_INVALID_URL);
        }

        SchannelTlsStream stream;
        if (!stream.Connect(target.host, target.port, target.tls, conn_timeout, rw_timeout)) {
            return fail(stream.LastError());
        }
        if (!PublishTls(&stream)) {
            return fail(0);
        }

        std::string request = method + " " + target.target + " HTTP/1.1\r\nHost: " + target.host_header +
                              "\r\nUser-Agent: " + user_agent + "\r\nConnection: close\r\n";
        if (!headers.empty()) {
            request += headers + "\r\n";
        }
        if (!body.empty() || method == "POST" || method == "PUT") {
            request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        }
        request += "\r\n";

        std::string head;
        std::string leftover;
        const bool responded = stream.Send(request.data(), request.size()) &&
                               (body.empty() || stream.Send(body.data(), body.size())) &&
                               ReadHead(stream, head, leftover);
        if (!responded) {
            PublishTls(nullptr);
            return fail(stream.LastError());
        }

        const int status = ParseStatusCode(head);
        if (status == 0) {
            PublishTls(nullptr);
            return fail(ERROR_WINHTTP_INVALID_SERVER_RESPONSE);
        }

        const std::string location = FindHeader(head, "Location");
        const bool redirect = (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) &&
                              !location.empty() && redirects < max_redirects;
        if (redirect) {
            PublishTls(nullptr);
            if ((status == 301 || status == 302 || status == 303) && method == "POST") {
                method = "GET";
                body.clear();
            }
            url = ResolveRedirect(target, location);
            continue;
        }

        m_StatusCode = status;
        OnHeader(head.c_str(), head.size());

        const bool has_body = !m_NoBody && method != "HEAD" && status != 204 && status != 304;
        HttpBodyReader::Framing framing = HttpBodyReader::Framing::Length;
        uint64_t length = 0;
        if (has_body) {
            const std::string transfer_encoding = FindHeader(head, "Transfer-Encoding");
            const std::string content_length = FindHeader(head, "Content-Length");
            if (transfer_encoding.find("chunked") != std::string::npos) {
                framing = HttpBodyReader::Framing::Chunked;
            }
            else if (!content_length.empty()) {
                length = strtoull(content_length.c_str(), nullptr, 10);
            }
            else {
                framing = HttpBodyReader::Framing::UntilClose;
            }
        }

        HttpBodyReader reader(stream, std::move(leftover), framing, length);
        char buf[8192];
        for (;;) {
            const int n = reader.Read(buf, sizeof(buf));
            if (n < 0) {
                PublishTls(nullptr);
                return fail(stream.LastError());
            }
            if (n == 0) {
                break;
            }
            OnContent(buf, static_cast<size_t>(n));
        }
        PublishTls(nullptr);

        if (m_Aborted.load()) {
            m_Status = ResponseStatus::Aborted;
            return false;
        }
        m_Status = ResponseStatus::Completed;
        OnPerformed();
        return true;
    }
}

const char* HttpRequest::GetStatusStr() const
{
    switch (m_Status) {
        case ResponseStatus::None:      return "None";
        case ResponseStatus::Completed: return "Completed";
        case ResponseStatus::Error:     return "Error";
        case ResponseStatus::TimedOut:  return "TimedOut";
        case ResponseStatus::Aborted:   return "Aborted";
        default:                        return "Unknown";
    }
}

std::string HttpRequest::GetErrorDescription() const
{
    if (m_Status == ResponseStatus::Completed) return {};

    std::string out = GetStatusStr();
    if (m_StatusCode) {
        char code[32];
        snprintf(code, sizeof(code), " HTTP %d", m_StatusCode);
        out += code;
    }
    if (m_TransportError) {
        wchar_t* message = nullptr;
        const DWORD len = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS,
            GetModuleHandleW(L"winhttp.dll"), m_TransportError,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&message), 0, nullptr);
        std::string text = len && message ? WideToUtf8(message, len) : std::string{};
        if (message) LocalFree(message);
        while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' ')) {
            text.pop_back();
        }
        char code[32];
        snprintf(code, sizeof(code), " (error %u", m_TransportError);
        out += code;
        if (!text.empty()) out += ": " + text;
        out += ")";
    }
    return out;
}

const char* HttpRequest::GetProtocolPrefix(const Protocol proto)
{
    switch (proto) {
        case Protocol::Ftp:   return "ftp://";
        case Protocol::File:  return "file://";
        case Protocol::Http:  return "http://";
        case Protocol::Https: return "https://";
        default:              return "";
    }
}

const char* HttpRequest::GetHttpMethodName(const HttpMethod method)
{
    switch (method) {
        case HttpMethod::Get:  return "GET";
        case HttpMethod::Put:  return "PUT";
        case HttpMethod::Post: return "POST";
        default:               return "";
    }
}

void HttpRequest::OnHeader(const char* bytes, const size_t count)
{
    m_Header.append(bytes, count);
}

void HttpRequest::OnContent(const char* bytes, const size_t count)
{
    m_Content.append(bytes, count);
}

void ComposeUrl(std::string& url, const char* host, const char* path)
{
    url.append(host);
    if (!EndsWith(url, "/")) {
        url.append("/");
    }
    if (*path != 0 && StartsWith(path, "/")) {
        ++path;
    }
    url.append(path);
}

void ComposeUrl(std::string& url, const Protocol proto,
                const char* host, const char* path)
{
    const char* prefix = HttpRequest::GetProtocolPrefix(proto);
    if (!StartsWith(host, prefix)) {
        url.append(prefix);
    }
    ComposeUrl(url, host, path);
}

bool EscapeUrl(std::string& url, const char* pUrl)
{
    if (!pUrl) return false;
    url.clear();
    auto is_unreserved = [](unsigned char c) {
        return (c >= 'A' && c <= 'Z')
            || (c >= 'a' && c <= 'z')
            || (c >= '0' && c <= '9')
            || c == '-' || c == '_' || c == '.' || c == '~';
    };
    static const char hex[] = "0123456789ABCDEF";
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(pUrl); *p; ++p) {
        if (is_unreserved(*p)) {
            url.push_back(static_cast<char>(*p));
        }
        else {
            url.push_back('%');
            url.push_back(hex[(*p >> 4) & 0xF]);
            url.push_back(hex[*p & 0xF]);
        }
    }
    return true;
}
