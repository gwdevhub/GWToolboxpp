#pragma once

// Comment and put in precompiled header if you want :)
#include <stdint.h>
#include <string>
#include <initializer_list>

#if defined(CURL_STRICTER)
typedef struct Curl_easy CURL;
typedef struct Curl_multi CURLM;
#else
typedef void CURL;
typedef void CURLM;
#endif

void InitCurl();
void ShutdownCurl();

struct UploadBuffer
{
    const uint8_t  *data;
    size_t          size;
    size_t          rpos;
};

enum class HttpMethod
{
    Get,
    Put,
    Post,
};

enum class ContentFlag
{
    Copy = 0,
    ByRef = 1,
};

enum class Protocol
{
    None,
    Ftp,
    File,
    Http,
    Https,
};

enum class ResponseStatus
{
    None,
    Completed,
    Error,
    TimedOut,
    Aborted,
};

typedef std::pair<const char *, const char *> ParamField;
class CurlEasy
{
    friend class CurlMulti;
public:
    CurlEasy();
    virtual ~CurlEasy();

    void SetUrl(const char *url);
    void SetUrl(Protocol proto, const char *url);
    void SetPort(uint16_t port);
    void SetMethod(const char *method);
    void SetMethod(HttpMethod method);
    void SetHeader(const char *field); // Format is "Name: Value"
    void SetHeader(const char *name, const char *value);
    void SetHeaders(std::initializer_list<ParamField> headers);
    void SetUserAgent(const char *user_agent);
    void SetTimeoutMs(int timeout_ms);
    void SetTimeoutSec(int timeout_sec);
    // Transfer aborted if slower than MinBytes/sec during TimeSec seconds,
    // effectively setting m_Status to ResponseStatus::TimedOut
    void SetLowSpeedLimit(int MinBytes, int TimeSec);
    void SetMaxRedirects(int amount);
    void SetNoBody(bool enable);
    void SetTcpNoDelay(bool enable);
    void SetVerifyPeer(bool enable);
    void SetVerifyHost(bool enable);
    void SetFollowLocation(bool enable);
    void SetProxy(const char *url);
    void SetProxy(const char *url, uint16_t port);
    void SetProxyAuth(const char *username, const char *password);
    void SetBufferSize(size_t size);
    void SetPostContent(std::string& content, ContentFlag flag);
    void SetPostContent(const char *content, size_t size, ContentFlag flag);
    void SetUploadBuffer(std::string& content, ContentFlag flag);
    void SetUploadBuffer(const char *content, size_t size, ContentFlag flag);
    void SetUploadFile(FILE *file);
    void SetUploadFile(FILE *file, size_t size);
    void SetUploadFile(const char *path);

    // Clear the response data and status flag
    void Clear();

    // Call "curl_easy_reset" and similarly reset the internal option of "CurlEasy"
    void Reset();

    // Perform a blocking curl request
    bool Perform();

    std::string&    GetHeader()     { return m_Header; };
    std::string&    GetContent()    { return m_Content; }

    CURL*           GetHandle()     { return m_Handle; }
    ResponseStatus  GetStatus()     { return m_Status; }
    int             GetStatusCode() { return m_StatusCode; }

    const char*     GetStatusStr();

#ifndef _NDEBUG
    const char*     GetErrorStr()   { return m_ErrorBuffer; }
#endif

    static const char* GetProtocolPrefix(Protocol Proto);
    static const char* GetHttpMethodName(HttpMethod Method);

protected:
    void UpdateStatus(int CurlStatus);

    // OnPerformed is executed in the context of the global curl thread which means
    // that you need to be careful when destruction the object. If OnPerformed access
    // any member variable, be sure to call Abort before the member variable start
    // destructing. This could be within the destructor itself, because Abort guarantee
    // that you can safely destruct the object when it returns.
    virtual void OnPerformed() {}

    // A sub-class can call those callback if they want to keep in memory
    // the body content and header content in the CurlEasy object.
    // This come with a performance penality caused by the dynamic allocation.
    virtual void OnHeader(const char *bytes, size_t count);
    virtual void OnContent(const char *bytes, size_t count);

private:
    static size_t WriteCallback(char *buffer, size_t size, size_t nitems, void *userdata);
    static size_t HeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata);
    
    static size_t ReadFileCallback(char *buffer, size_t size, size_t nitems, void *userdata);
    static size_t ReadBufferCallback(char *buffer, size_t size, size_t nitems, void *userdata);

    // You are encouraged to re-write this function however you want, this
    // case represent the use cases of having a lot of hardcoded value where
    // an assert will catch all errors. Hence, we don't need to properly deal
    // with those errors in release.
    static void HandleOptionError(CurlEasy *Handle, int Code, const char *File, unsigned Line);

    CurlEasy& operator=(const CurlEasy&) = delete;

protected:
    CURL               *m_Handle;
    struct curl_slist  *m_Headers;

    // "m_File" is closed on "Reset"
    FILE               *m_File;
    FILE               *m_UploadFile;

    std::string         m_UploadContent;
    UploadBuffer        m_UploadBuffer;

    // Those std::string can be replaced, but keep in mind that the small string optimisation
    // make those free if you don't use them (i.e. override OnHeader & OnContent) and that
    // they are prety good at growing.
    std::string         m_Header;
    std::string         m_Content;
    ResponseStatus      m_Status;
    int                 m_StatusCode;

#ifndef _NDEBUG
    char                m_ErrorBuffer[256];
#endif

private:
    // In libcurl, when a easy handle isn't attached to a multi handle and is
    // removed from it (i.e. curl_multi_remove_handle) CURLE_OK is returned so,
    // there is no way to deal with it without keeping track of it ourself.
    // We rely on the fact that it's not possible to attach an "easy handle"
    // to more than one "multi handle" to keep track of if it.
    // Moreover, this could very well be a boolean and maybe should be one, but
    // this give more informations (for debugging typically) that can be useful.
    CURLM *m_MultiHandle;
};

class CurlMulti
{
public:
    CurlMulti();
    virtual ~CurlMulti();

    CURLM* GetHandle() { return m_Handle; }

    // AddHandle, RemoveHandle are as thread-safe as curl_multi_add_handle & curl_multi_remove_handle
    void AddHandle(CurlEasy *Handle);
    void RemoveHandle(CurlEasy *Handle);

    void Perform();

protected:
    CURLM *m_Handle;

    CurlMulti& operator=(const CurlMulti&) = delete;
};

void ComposeUrl(std::string& url, const char* host, const char* path);
void ComposeUrl(std::string& url, Protocol proto,
                const char* host, const char* path);
bool EscapeUrl(std::string& url, const char* pUrl);

#ifdef _NDEBUG
# define CHECK_CURL_EASY_SETOPT(handle, ...) CurlEasy::HandleOptionError(handle, curl_easy_setopt(handle->m_Handle, __VA_ARGS__), nullptr, 0)
#else
# define CHECK_CURL_EASY_SETOPT(handle,  ...) CurlEasy::HandleOptionError(handle, curl_easy_setopt(handle->m_Handle, __VA_ARGS__), __FILE__, __LINE__)
#endif