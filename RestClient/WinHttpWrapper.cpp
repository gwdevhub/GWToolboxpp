#include "WinHttpWrapper.h"
#include "stdafx.h"

#include <string>
#include <vector>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

    // ---------------------------------------------------------------------------
// No-ops — curl had a global init/shutdown, WinHTTP doesn't need one
// ---------------------------------------------------------------------------
void InitCurl() {}
void ShutdownCurl() {}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

    std::wstring Widen(const std::string& s)
    {
        if (s.empty()) return {};
        const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
        std::wstring out(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), len);
        return out;
    }

    std::string Narrow(const std::wstring& s)
    {
        if (s.empty()) return {};
        const int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
        std::string out(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), len, nullptr, nullptr);
        return out;
    }

    struct ParsedUrl {
        std::wstring scheme;
        std::wstring host;
        INTERNET_PORT port = INTERNET_DEFAULT_PORT;
        std::wstring path;
    };

    bool ParseUrl(const std::wstring& url, ParsedUrl& out)
    {
        URL_COMPONENTSW uc{};
        uc.dwStructSize = sizeof(uc);
        wchar_t scheme[16]{}, host[256]{}, path[4096]{};
        uc.lpszScheme = scheme;
        uc.dwSchemeLength = _countof(scheme);
        uc.lpszHostName = host;
        uc.dwHostNameLength = _countof(host);
        uc.lpszUrlPath = path;
        uc.dwUrlPathLength = _countof(path);

        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) return false;

        out.scheme = scheme;
        out.host = host;
        out.port = uc.nPort;
        out.path = path;
        if (uc.lpszExtraInfo && uc.dwExtraInfoLength) out.path += uc.lpszExtraInfo;
        return true;
    }

    // Build multipart/form-data body, returns the boundary string
    std::string BuildMultipart(const std::vector<MimePart>& parts, std::string& body)
    {
        const std::string boundary = "----GWToolboxFormBoundary7MA4YWxkTrZu0gW";
        const std::string crlf = "\r\n";
        body.clear();
        for (const auto& part : parts) {
            body += "--" + boundary + crlf;
            body += "Content-Disposition: form-data; name=\"" + part.name + "\"";
            if (!part.filename.empty()) body += "; filename=\"" + part.filename + "\"";
            body += crlf;
            if (!part.content_type.empty()) body += "Content-Type: " + part.content_type + crlf;
            body += crlf;
            body.append(part.data);
            body += crlf;
        }
        body += "--" + boundary + "--" + crlf;
        return boundary;
    }

} // namespace

// ---------------------------------------------------------------------------
// CurlEasy
// ---------------------------------------------------------------------------
CurlEasy::CurlEasy() : m_Method("GET"), m_UserAgent("GWToolboxpp") {}

void CurlEasy::SetUrl(const char* url)
{
    m_Url = url ? url : "";
}
void CurlEasy::SetUrl(Protocol proto, const char* url)
{
    m_Url = std::string(GetProtocolPrefix(proto)) + (url ? url : "");
}
void CurlEasy::SetPort(uint16_t port)
{
    m_Port = port;
}
void CurlEasy::SetMethod(const char* method)
{
    m_Method = method ? method : "GET";
}
void CurlEasy::SetMethod(HttpMethod method)
{
    m_Method = GetHttpMethodName(method);
}
void CurlEasy::SetUserAgent(const char* ua)
{
    m_UserAgent = ua ? ua : "";
}
void CurlEasy::SetTimeoutMs(int ms)
{
    m_TimeoutMs = ms;
}
void CurlEasy::SetTimeoutSec(int s)
{
    m_TimeoutMs = s * 1000;
}
void CurlEasy::SetConnectTimeoutMs(int ms)
{
    m_ConnectTimeoutMs = ms;
}
void CurlEasy::SetConnectTimeoutSec(int s)
{
    m_ConnectTimeoutMs = s * 1000;
}
void CurlEasy::SetLowSpeedLimit(int, int) {}
void CurlEasy::SetMaxRedirects(int amount)
{
    m_MaxRedirects = amount;
}
void CurlEasy::SetNoBody(bool enable)
{
    m_NoBody = enable;
}
void CurlEasy::SetTcpNoDelay(bool) {}
void CurlEasy::SetVerifyPeer(bool enable)
{
    m_VerifyPeer = enable;
}
void CurlEasy::SetVerifyHost(bool enable)
{
    m_VerifyHost = enable;
}
void CurlEasy::SetFollowLocation(bool enable)
{
    m_FollowLocation = enable;
}
void CurlEasy::SetBufferSize(size_t) {}
void CurlEasy::SetProxy(const char*) {}
void CurlEasy::SetProxy(const char*, uint16_t) {}
void CurlEasy::SetProxyAuth(const char*, const char*) {}

void CurlEasy::SetHeader(const char* field)
{
    if (field) m_ExtraHeaders.emplace_back(field);
}

void CurlEasy::SetHeader(const char* name, const char* value)
{
    if (name && value) m_ExtraHeaders.emplace_back(std::string(name) + ": " + value);
}

void CurlEasy::SetHeaders(std::initializer_list<ParamField> headers)
{
    for (auto& [name, value] : headers)
        SetHeader(name, value);
}

void CurlEasy::SetPostContent(const std::string& content, ContentFlag)
{
    m_PostContent = content;
}

void CurlEasy::SetPostContent(const char* content, size_t size, ContentFlag)
{
    m_PostContent.assign(content, size);
}

void CurlEasy::AddMimePart(const char* name, const char* data, size_t size, const char* filename, const char* content_type)
{
    MimePart part;
    part.name = name ? name : "";
    part.data.assign(data, size);
    part.filename = filename ? filename : "";
    part.content_type = content_type ? content_type : "";
    m_MimeParts.push_back(std::move(part));
}

void CurlEasy::ClearMimeParts()
{
    m_MimeParts.clear();
}

void CurlEasy::Clear()
{
    m_Header.clear();
    m_Content.clear();
    m_Status = ResponseStatus::None;
    m_StatusCode = 0;
}

void CurlEasy::Reset()
{
    Clear();
    m_Url.clear();
    m_Method = "GET";
    m_UserAgent = "GWToolboxpp";
    m_PostContent.clear();
    m_MimeParts.clear();
    m_ExtraHeaders.clear();
    m_Port = 0;
    m_TimeoutMs = 10000;
    m_ConnectTimeoutMs = 5000;
    m_MaxRedirects = 10;
    m_VerifyPeer = true;
    m_VerifyHost = true;
    m_FollowLocation = true;
    m_NoBody = false;
}

void CurlEasy::OnHeader(const char* bytes, size_t count)
{
    m_Header.append(bytes, count);
}
void CurlEasy::OnContent(const char* bytes, size_t count)
{
    m_Content.append(bytes, count);
}

void CurlEasy::UpdateStatus(int curlStatus)
{
    m_Status = (curlStatus == 0) ? ResponseStatus::Completed : ResponseStatus::Error;
}

bool CurlEasy::Perform()
{
    Clear();

    ParsedUrl parsed;
    if (!ParseUrl(Widen(m_Url), parsed)) {
        m_Status = ResponseStatus::Error;
        return false;
    }

    const bool isHttps = (parsed.scheme == L"https");
    const INTERNET_PORT port = m_Port ? static_cast<INTERNET_PORT>(m_Port) : parsed.port;

    HINTERNET hSession = WinHttpOpen(Widen(m_UserAgent).c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        m_Status = ResponseStatus::Error;
        return false;
    }

    WinHttpSetTimeouts(hSession, m_ConnectTimeoutMs, m_ConnectTimeoutMs, m_TimeoutMs, m_TimeoutMs);

    HINTERNET hConnect = WinHttpConnect(hSession, parsed.host.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        m_Status = ResponseStatus::Error;
        return false;
    }

    const std::wstring wPath = parsed.path.empty() ? L"/" : parsed.path;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, Widen(m_Method).c_str(), wPath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, isHttps ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        m_Status = ResponseStatus::Error;
        return false;
    }

    // SSL verification
    if (!m_VerifyPeer || !m_VerifyHost) {
        DWORD secFlags = SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
    }

    // Redirect policy
    DWORD redirectPolicy = m_FollowLocation ? WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS : WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    // Body — mime parts take priority over raw post content
    std::string body;
    std::string mimeContentTypeHeader;
    if (!m_MimeParts.empty()) {
        const std::string boundary = BuildMultipart(m_MimeParts, body);
        mimeContentTypeHeader = "Content-Type: multipart/form-data; boundary=" + boundary;
    }
    else {
        body = m_PostContent;
    }

    // Headers
    {
        std::wstring combined;
        for (const auto& h : m_ExtraHeaders) {
            combined += Widen(h);
            combined += L"\r\n";
        }
        if (!mimeContentTypeHeader.empty()) {
            combined += Widen(mimeContentTypeHeader);
            combined += L"\r\n";
        }
        if (!combined.empty()) {
            WinHttpAddRequestHeaders(hRequest, combined.c_str(), static_cast<DWORD>(-1L), WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        }
    }

    const DWORD bodySize = static_cast<DWORD>(body.size());
    bool ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, body.empty() ? nullptr : const_cast<char*>(body.data()), bodySize, bodySize, 0) != FALSE;

    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr) != FALSE;

    if (!ok) {
        const DWORD err = GetLastError();
        m_Status = (err == ERROR_WINHTTP_TIMEOUT) ? ResponseStatus::TimedOut : ResponseStatus::Error;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Status code
    DWORD statusCode = 0, statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    m_StatusCode = static_cast<int>(statusCode);

    // Response headers
    DWORD headerSize = 0;
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &headerSize, WINHTTP_NO_HEADER_INDEX);
    if (headerSize > 0) {
        std::wstring wHeaders(headerSize / sizeof(wchar_t), L'\0');
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, wHeaders.data(), &headerSize, WINHTTP_NO_HEADER_INDEX);
        const auto narrow = Narrow(wHeaders);
        OnHeader(narrow.data(), narrow.size());
    }

    // Response body
    if (!m_NoBody) {
        DWORD bytesAvailable = 0;
        while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
            std::string chunk(bytesAvailable, '\0');
            DWORD bytesRead = 0;
            if (!WinHttpReadData(hRequest, chunk.data(), bytesAvailable, &bytesRead)) break;
            OnContent(chunk.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    m_Status = ResponseStatus::Completed;
    return true;
}

const char* CurlEasy::GetStatusStr()
{
    switch (m_Status) {
        case ResponseStatus::Completed:
            return "Completed";
        case ResponseStatus::Error:
            return "Error";
        case ResponseStatus::TimedOut:
            return "TimedOut";
        case ResponseStatus::Aborted:
            return "Aborted";
        default:
            return "None";
    }
}

const char* CurlEasy::GetProtocolPrefix(Protocol proto)
{
    switch (proto) {
        case Protocol::Http:
            return "http://";
        case Protocol::Https:
            return "https://";
        case Protocol::Ftp:
            return "ftp://";
        case Protocol::File:
            return "file://";
        default:
            return "";
    }
}

const char* CurlEasy::GetHttpMethodName(HttpMethod method)
{
    switch (method) {
        case HttpMethod::Post:
            return "POST";
        case HttpMethod::Put:
            return "PUT";
        default:
            return "GET";
    }
}

// ---------------------------------------------------------------------------
// URL helpers
// ---------------------------------------------------------------------------
void ComposeUrl(std::string& url, const char* host, const char* path)
{
    url = std::string("https://") + host + path;
}

void ComposeUrl(std::string& url, Protocol proto, const char* host, const char* path)
{
    url = std::string(CurlEasy::GetProtocolPrefix(proto)) + host + path;
}

bool EscapeUrl(std::string& url, const char* pUrl)
{
    if (!pUrl) return false;
    url = pUrl; // passthrough — WinHTTP handles encoding internally
    return true;
}
