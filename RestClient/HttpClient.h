#pragma once

#include <stdint.h>
#include <atomic>
#include <string>
#include <utility>
#include <initializer_list>

void InitHttpClient();
void ShutdownHttpClient();

// Backwards-compatible aliases used by existing callers.
inline void InitCurl() { InitHttpClient(); }
inline void ShutdownCurl() { ShutdownHttpClient(); }

enum class HttpMethod {
    Get,
    Put,
    Post,
};

enum class ContentFlag {
    Copy  = 0,
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

class HttpRequest {
public:
    HttpRequest();
    HttpRequest(const HttpRequest&) = delete;
    HttpRequest& operator=(const HttpRequest&) = delete;

    virtual ~HttpRequest();

    void SetUrl(const char* url);
    void SetUrl(Protocol proto, const char* url);
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
    void SetMaxRedirects(int amount);
    void SetNoBody(bool enable);
    void SetVerifyPeer(bool enable);
    void SetVerifyHost(bool enable);
    void SetFollowLocation(bool enable);
    void SetProxy(const char* url);
    void SetPostContent(const std::string& content, ContentFlag flag);
    void SetPostContent(const char* content, size_t size, ContentFlag flag);

    // Clear the response data and status flag
    void Clear();

    // Reset all configuration
    void Reset();

    // Perform a blocking HTTPS request
    bool Perform();

    // Thread-safe: cancels an in-flight Perform() running on another thread.
    void RequestAbort();

    std::string& GetHeader() { return m_Header; }
    std::string& GetContent() { return m_Content; }

    ResponseStatus GetStatus() const { return m_Status; }
    int GetStatusCode() const { return m_StatusCode; }

    const char* GetStatusStr() const;

    static const char* GetProtocolPrefix(Protocol proto);
    static const char* GetHttpMethodName(HttpMethod method);

protected:
    virtual void OnPerformed() {}

    // A sub-class can override these to capture body / header bytes as they arrive.
    virtual void OnHeader(const char* bytes, size_t count);
    virtual void OnContent(const char* bytes, size_t count);

protected:
    std::string m_Header;
    std::string m_Content;
    ResponseStatus m_Status;
    int m_StatusCode;

private:
    void CloseHandles();

    // Configuration (set via SetXxx)
    std::wstring m_Url;
    std::wstring m_UserAgent;
    std::wstring m_Method;
    std::wstring m_HeadersBlock; // CRLF-separated header lines
    std::wstring m_Proxy;
    std::string m_PostBodyStorage; // backing store for Copy mode
    const void* m_PostBody;
    size_t m_PostBodySize;

    int m_TimeoutMs;
    int m_ConnectTimeoutMs;
    int m_MaxRedirects;
    bool m_FollowLocation;
    bool m_VerifyPeer;
    bool m_VerifyHost;
    bool m_NoBody;
    bool m_HasCustomMethod;

    // WinHTTP handles - opaque (void*) to avoid pulling winhttp.h into the header.
    void* m_hSession;
    void* m_hConnect;
    void* m_hRequest;

    // Cancellation
    void* m_CancelMutex; // CRITICAL_SECTION*
    std::atomic<bool> m_Aborted;
};

// Backwards-compatible alias for the previous class name.
using CurlEasy = HttpRequest;

void ComposeUrl(std::string& url, const char* host, const char* path);
void ComposeUrl(std::string& url, Protocol proto, const char* host, const char* path);
bool EscapeUrl(std::string& url, const char* pUrl);
