#pragma once

#include <initializer_list>
#include <stdint.h>
#include <string>
#include <vector>

void InitCurl();     // no-op, kept for compatibility
void ShutdownCurl(); // no-op, kept for compatibility

struct UploadBuffer {
    const uint8_t* data;
    size_t size;
    size_t rpos;
};

enum class HttpMethod {
    Get,
    Put,
    Post,
};

enum class ContentFlag {
    Copy = 0,
    ByRef = 1,
};

enum class Protocol {
    None,
    Ftp,
    File,
    Http,
    Https,
};

enum class ResponseStatus {
    None,
    Completed,
    Error,
    TimedOut,
    Aborted,
};

using ParamField = std::pair<const char*, const char*>;

struct MimePart {
    std::string name;
    std::string data;
    std::string filename;
    std::string content_type;
};

class CurlEasy {
public:
    CurlEasy();
    virtual ~CurlEasy() = default;

    void SetUrl(const char* url);
    void SetUrl(Protocol proto, const char* url);
    void SetPort(uint16_t port);
    void SetMethod(const char* method);
    void SetMethod(HttpMethod method);
    void SetHeader(const char* field); // Format is "Name: Value"
    void SetHeader(const char* name, const char* value);
    void SetHeaders(std::initializer_list<ParamField> headers);
    void SetUserAgent(const char* user_agent);
    void SetTimeoutMs(int timeout_ms);
    void SetTimeoutSec(int timeout_sec);
    void SetConnectTimeoutMs(int timeout_ms);
    void SetConnectTimeoutSec(int timeout_sec);
    void SetLowSpeedLimit(int MinBytes, int TimeSec); // no-op on WinHTTP
    void SetMaxRedirects(int amount);
    void SetNoBody(bool enable);
    void SetTcpNoDelay(bool enable);
    void SetVerifyPeer(bool enable);
    void SetVerifyHost(bool enable);
    void SetFollowLocation(bool enable);
    void SetProxy(const char* url);
    void SetProxy(const char* url, uint16_t port);
    void SetProxyAuth(const char* username, const char* password);
    void SetBufferSize(size_t size); // no-op on WinHTTP
    void SetPostContent(const std::string& content, ContentFlag flag);
    void SetPostContent(const char* content, size_t size, ContentFlag flag);

    // Multipart form support (replaces curl_mime_*)
    void AddMimePart(const char* name, const char* data, size_t size, const char* filename = nullptr, const char* content_type = nullptr);
    void ClearMimeParts();

    // Not implemented (not used by Resources.cpp):
    void SetUploadBuffer(const std::string&, ContentFlag) {}
    void SetUploadBuffer(const char*, size_t, ContentFlag) {}
    void SetUploadFile(FILE*) {}
    void SetUploadFile(FILE*, size_t) {}
    void SetUploadFile(const char*) {}

    void Clear();
    void Reset();

    bool Perform();

    std::string& GetHeader() { return m_Header; }
    std::string& GetContent() { return m_Content; }

    void* GetHandle() const { return nullptr; } // no handle in WinHTTP impl
    ResponseStatus GetStatus() const { return m_Status; }
    int GetStatusCode() const { return m_StatusCode; }
    const char* GetStatusStr();

    static const char* GetProtocolPrefix(Protocol proto);
    static const char* GetHttpMethodName(HttpMethod method);

protected:
    virtual void OnPerformed() {}
    virtual void OnHeader(const char* bytes, size_t count);
    virtual void OnContent(const char* bytes, size_t count);

    // Kept as int to match original CurlEasy::UpdateStatus(int CurlStatus) signature.
    // Pass 0 for success (mirrors CURLE_OK), non-zero for error.
    void UpdateStatus(int curlStatus);

    std::string m_Url;
    std::string m_Method; // "GET", "POST", "PUT"
    std::string m_UserAgent;
    std::string m_PostContent;
    std::string m_Header;
    std::string m_Content;

    // Extra headers as "Name: Value" strings
    std::vector<std::string> m_ExtraHeaders;

    std::vector<MimePart> m_MimeParts;

    uint16_t m_Port = 0;
    int m_TimeoutMs = 10000;
    int m_ConnectTimeoutMs = 5000;
    int m_MaxRedirects = 10;
    bool m_VerifyPeer = true;
    bool m_VerifyHost = true;
    bool m_FollowLocation = true;
    bool m_NoBody = false;

    ResponseStatus m_Status = ResponseStatus::None;
    int m_StatusCode = 0;
};

// Stubs kept so nothing referencing CurlMulti hard-breaks at compile time,
// but AsyncRestClient/CurlMultiThread are not functional without libcurl.
// Resources.cpp doesn't use async, so this is fine.
class CurlMulti {
public:
    CurlMulti() = default;
    ~CurlMulti() = default;
    void AddHandle(CurlEasy*) const {}
    void RemoveHandle(CurlEasy*) const {}
    void Perform() const {}
    void* GetHandle() const { return nullptr; }
};

void ComposeUrl(std::string& url, const char* host, const char* path);
void ComposeUrl(std::string& url, Protocol proto, const char* host, const char* path);
bool EscapeUrl(std::string& url, const char* pUrl);
