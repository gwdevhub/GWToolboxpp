#include "stdafx.h"

#include "CurlWrapper.h"

#ifdef _NDEBUG
# define CHECK_CURL_EASY_SETOPT(handle, ...) CurlEasy::HandleOptionError(handle, curl_easy_setopt(handle->m_Handle, __VA_ARGS__), nullptr, 0)
#else
# define CHECK_CURL_EASY_SETOPT(handle, ...) CurlEasy::HandleOptionError(handle, curl_easy_setopt(handle->m_Handle, __VA_ARGS__), __FILE__, __LINE__)
#endif

static bool StartsWith(const char *str, const char *prefix)
{
    for (; *prefix != 0; ++prefix, ++str) {
        if (*prefix != *str)
            return false;
    }
    return true;
}

static bool EndsWith(const std::string& str, const char *suffix)
{
    size_t suffix_len = strlen(suffix);
    if (str.size() < suffix_len)
        return false;
    size_t str_start = str.size() - suffix_len;
    for (size_t i = 0; i < suffix_len; ++i) {
        if (str[str_start + i] != suffix[i])
            return false;
    }
    return true;
}

static bool GetFileSize(FILE *file, curl_off_t *size)
{
    long pos = ftell(file);
    if (pos < 0)
        return false;
    if (fseek(file, 0, SEEK_END) != 0)
        return false;
    long file_size = ftell(file);
    fseek(file, pos, SEEK_SET);
    if (file_size >= 0) {
        *size = static_cast<curl_off_t>(file_size);
        return true;
    } else {
        return false;
    }
}

static std::atomic<int> InitializeCount;
void InitCurl()
{
    if (++InitializeCount == 1)
        curl_global_init(CURL_GLOBAL_ALL);
}

void ShutdownCurl()
{
    assert(InitializeCount > 0);
    if (--InitializeCount == 0)
        curl_global_cleanup();
}

CurlEasy::CurlEasy()
    : m_Headers(nullptr)
    , m_File(nullptr)
    , m_UploadFile(nullptr)
    , m_Status(ResponseStatus::None)
    , m_StatusCode(0)
    , m_MultiHandle(nullptr)
{
#ifndef _NDEBUG
    static_assert(sizeof(CurlEasy::m_ErrorBuffer) >= CURL_ERROR_SIZE,
        "CurlEasy::m_ErrorBuffer must be at least CURL_ERROR_SIZE big. (See https://curl.haxx.se/libcurl/c/CURLOPT_ERRORBUFFER.html)");
#endif

    assert(InitializeCount > 0);
    m_Handle = curl_easy_init();
    Reset();
}

CurlEasy::~CurlEasy()
{
    Reset();
    curl_easy_cleanup(m_Handle);
}

void CurlEasy::SetUrl(Protocol proto, const char *url)
{
    const char *prefix = GetProtocolPrefix(proto);
    if (StartsWith(url, prefix))
        return SetUrl(url);
    // @Enhancement: Replace by proper string builder.
    std::string temp;
    temp.append(prefix);
    temp.append(temp);
    return SetUrl(temp.c_str());
}

void CurlEasy::SetUrl(const char *url)
{
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_URL, url);
}

void CurlEasy::SetPort(uint16_t port)
{
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_PORT, static_cast<long>(port));
}

void CurlEasy::SetMethod(const char *method)
{
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_CUSTOMREQUEST, method);
}

void CurlEasy::SetMethod(HttpMethod method)
{
    switch (method)
    {
    case HttpMethod::Get:
        CHECK_CURL_EASY_SETOPT(this, CURLOPT_HTTPGET, 1L);
        break;
    case HttpMethod::Put:
        CHECK_CURL_EASY_SETOPT(this, CURLOPT_PUT, 1L);
        break;
    case HttpMethod::Post:
        CHECK_CURL_EASY_SETOPT(this, CURLOPT_POST, 1L);
        break;
    }
}

void CurlEasy::SetHeader(const char *field)
{
    assert(field != nullptr);
    struct curl_slist *temp = curl_slist_append(m_Headers, field);
    assert(temp != nullptr);
    if (temp)
        m_Headers = temp;
}

void CurlEasy::SetHeader(const char *name, const char *value)
{
    assert(name && value);

    // @Enhancement: Replace by proper string builder.
    std::string HeaderEntry;
    HeaderEntry.append(name);
    HeaderEntry.append(":");
    HeaderEntry.append(value);
    SetHeader(HeaderEntry.c_str());
}

void CurlEasy::SetHeaders(std::initializer_list<ParamField> headers)
{
    for (ParamField header : headers)
        SetHeader(header.first, header.second);
}

void CurlEasy::SetUserAgent(const char *user_agent)
{
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_USERAGENT, user_agent);
}

void CurlEasy::SetTimeoutMs(int timeout_ms)
{
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_TIMEOUT_MS, static_cast<long>(timeout_ms));
}

void CurlEasy::SetTimeoutSec(int timeout_sec)
{
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_TIMEOUT, static_cast<long>(timeout_sec));
}

void CurlEasy::SetLowSpeedLimit(int MinBytes, int TimeSec)
{
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_LOW_SPEED_TIME, static_cast<long>(TimeSec));
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_LOW_SPEED_LIMIT, static_cast<long>(MinBytes));
}

void CurlEasy::SetMaxRedirects(int amount)
{
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_MAXREDIRS, static_cast<long>(amount));
}

void CurlEasy::SetNoBody(bool enable)
{
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_NOBODY, static_cast<long>(enable));
}

void CurlEasy::SetTcpNoDelay(bool enable)
{
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_TCP_NODELAY, static_cast<long>(enable));
}

void CurlEasy::SetVerifyPeer(bool enable)
{
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_SSL_VERIFYPEER, static_cast<long>(enable));
}
void CurlEasy::SetVerifyHost(bool enable)
{
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_SSL_VERIFYHOST, static_cast<long>(enable));
}

void CurlEasy::SetFollowLocation(bool enable)
{
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_FOLLOWLOCATION, static_cast<long>(enable));
}

void CurlEasy::SetProxy(const char *url)
{
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_PROXY, url);
}

void CurlEasy::SetProxy(const char *url, uint16_t port)
{
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_PROXY, url);
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_PROXYPORT, static_cast<long>(port));
}

void CurlEasy::SetProxyAuth(const char *username, const char *password)
{
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_PROXYUSERNAME, username);
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_PROXYPASSWORD, password);
}

void CurlEasy::SetBufferSize(size_t size)
{
    assert(1024 <= size && size <= CURL_MAX_READ_SIZE);
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_BUFFERSIZE, static_cast<long>(size));
}

void CurlEasy::SetPostContent(std::string& content, ContentFlag flag)
{
    return SetPostContent(content.c_str(), content.size(), flag);
}

void CurlEasy::SetPostContent(const char *content, size_t Size, ContentFlag flag)
{
    assert(content != nullptr);
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_POST, 1L);
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_POSTFIELDSIZE, static_cast<long>(Size));

    if (flag == ContentFlag::Copy) {
        CHECK_CURL_EASY_SETOPT(this, CURLOPT_COPYPOSTFIELDS, content);
    } else {
        CHECK_CURL_EASY_SETOPT(this, CURLOPT_POSTFIELDS, content);
    }
}

void CurlEasy::SetUploadBuffer(std::string& content, ContentFlag flag)
{
    return SetUploadBuffer(content.c_str(), content.size(), flag);
}

void CurlEasy::SetUploadBuffer(const char *content, size_t size, ContentFlag flag)
{
    assert(content && size);
    if (flag == ContentFlag::Copy) {
        m_UploadContent = std::string(content, content + size);
        m_UploadBuffer.data = reinterpret_cast<const uint8_t *>(m_UploadContent.c_str());
    } else {
        m_UploadBuffer.data = reinterpret_cast<const uint8_t *>(content);
    }

    m_UploadBuffer.size = size;
    m_UploadBuffer.rpos = 0;

    CHECK_CURL_EASY_SETOPT(this, CURLOPT_UPLOAD, 1L);
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_READFUNCTION, ReadBufferCallback);
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_READDATA, this);
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(size));
}

void CurlEasy::SetUploadFile(FILE *file)
{
    curl_off_t size;
    if (GetFileSize(file, &size)) {
        SetUploadFile(file, static_cast<size_t>(size));
    }
}

void CurlEasy::SetUploadFile(FILE *file, size_t size)
{
    m_UploadFile = file;
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_UPLOAD, 1L);
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_READFUNCTION, ReadFileCallback);
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_READDATA, this);
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(size));
}

void CurlEasy::SetUploadFile(const char *path)
{
    if (fopen_s(&m_File, path, "rb") != 0)
        SetUploadFile(m_File);
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
#ifndef _NDEBUG
    m_ErrorBuffer[0] = 0;
#endif

    m_UploadFile = nullptr;
    if (m_File) {
        fclose(m_File);
        m_File = nullptr;
    }

    m_UploadContent.clear();
    m_UploadBuffer.data = nullptr;
    m_UploadBuffer.size = 0;
    m_UploadBuffer.rpos = 0;

    if (m_Headers) {
        curl_slist_free_all(m_Headers);
        m_Headers = nullptr;
    }

    curl_easy_reset(m_Handle);
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_WRITEFUNCTION, WriteCallback);
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_WRITEDATA, this);
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_HEADERFUNCTION, HeaderCallback);
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_HEADERDATA, this);
#ifndef _NDEBUG
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_ERRORBUFFER, m_ErrorBuffer);
#endif
}

bool CurlEasy::Perform()
{
    Clear();
    CHECK_CURL_EASY_SETOPT(this, CURLOPT_HTTPHEADER, m_Headers);
    CURLcode status = curl_easy_perform(m_Handle);
    UpdateStatus(status);
    OnPerformed();
    return (status == CURLE_OK);
}

const char* CurlEasy::GetStatusStr()
{
    switch (GetStatus())
    {
    case ResponseStatus::None:
        return "None";
    case ResponseStatus::Completed:
        return "Completed";
    case ResponseStatus::Error:
        return "Error";
    case ResponseStatus::TimedOut:
        return "TimedOut";
    case ResponseStatus::Aborted:
        return "Aborted";
    default:
        return "Unknown";
    }
}

void CurlEasy::UpdateStatus(int CurlStatus)
{
    m_StatusCode = 0;
    long ResponseStatus;
    if (curl_easy_getinfo(m_Handle, CURLINFO_RESPONSE_CODE, &ResponseStatus) == CURLE_OK)
        m_StatusCode = ResponseStatus;

    switch (CurlStatus)
    {
    case CURLE_OK:
        m_Status = ResponseStatus::Completed;
        break;
    case CURLE_OPERATION_TIMEDOUT:
        m_Status = ResponseStatus::TimedOut;
        break;
    case CURLE_ABORTED_BY_CALLBACK:
        m_Status = ResponseStatus::Aborted;
        break;
    default:
        m_Status = ResponseStatus::Error;
        break;
    }
}

const char* CurlEasy::GetProtocolPrefix(Protocol proto)
{
    switch (proto)
    {
        case Protocol::Ftp:     return "ftp://";
        case Protocol::File:    return "file://";
        case Protocol::Http:    return "http://";
        case Protocol::Https:   return "https://";
        default: return "";
    };
}

const char* CurlEasy::GetHttpMethodName(HttpMethod method)
{
    switch (method)
    {
        case HttpMethod::Get:   return "GET";
        case HttpMethod::Put:   return "PUT";
        case HttpMethod::Post:  return "POST";
        default: return "";
    }
}

void CurlEasy::OnHeader(const char *bytes, size_t count)
{
    m_Header.append(bytes, count);
}

void CurlEasy::OnContent(const char *bytes, size_t count)
{
    m_Content.append(bytes, count);
}

size_t CurlEasy::WriteCallback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    CurlEasy *easy = static_cast<CurlEasy *>(userdata);
    size_t count = size * nitems;
    if ((count / size) == nitems) {
        easy->OnContent(buffer, count);
    }
    return count;
}

size_t CurlEasy::HeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    CurlEasy *easy = static_cast<CurlEasy *>(userdata);
    size_t count = size * nitems;
    assert((count / size) == nitems);
    if ((count / size) == nitems) {
        easy->OnHeader(buffer, count);
    }
    return count;
}

size_t CurlEasy::ReadFileCallback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    CurlEasy *easy = static_cast<CurlEasy *>(userdata);
    return fread(buffer, size, nitems, easy->m_UploadFile);
}

size_t CurlEasy::ReadBufferCallback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    CurlEasy *easy = static_cast<CurlEasy *>(userdata);
    UploadBuffer *stream = &easy->m_UploadBuffer;
    size_t count = size * nitems;
    size_t bytes = std::min<size_t>(count, stream->size - stream->rpos);
    memcpy(buffer, stream->data, bytes);
    stream->rpos += bytes;
    return bytes;
}

void CurlEasy::HandleOptionError(CurlEasy *easy, int code, const char *file, unsigned line)
{
    if (code != CURLE_OK) {
        file = file ? file : "<file>";
        fprintf(stderr, "curl_easy_setopt failed {CurlEasy: %p, code: %d, file: %s, line: %u}",
            easy, code, file, line);
    }
}

CurlMulti::CurlMulti()
{
    assert(InitializeCount > 0);
    m_Handle = curl_multi_init();
}

CurlMulti::~CurlMulti()
{
    curl_multi_cleanup(m_Handle);
}

void CurlMulti::AddHandle(CurlEasy *easy)
{
    assert(!easy->m_MultiHandle);

    CHECK_CURL_EASY_SETOPT(easy, CURLOPT_HTTPHEADER, easy->m_Headers);
    CURLMcode code = curl_multi_add_handle(m_Handle, easy->m_Handle);
    assert(code == CURLM_OK);
    if (code == CURLM_OK)
        easy->m_MultiHandle = this->m_Handle;
}

void CurlMulti::RemoveHandle(CurlEasy *easy)
{
    assert(easy->m_MultiHandle);

    CURLMcode code = curl_multi_remove_handle(m_Handle, easy->m_Handle);
    assert(code == CURLM_OK);
    if (code == CURLM_OK) {
        easy->m_MultiHandle = nullptr;
    }
}

void CurlMulti::Perform()
{
    int running_handles;
    CURLMcode code = curl_multi_perform(m_Handle, &running_handles);
    if (code != CURLM_OK) {
        fprintf(stderr, "Error in 'CurlMulti::Perform': %s\n", curl_multi_strerror(code));
    }
}

void ComposeUrl(std::string& url, const char* host, const char* path)
{
    url.append(host);
    if (!EndsWith(url, "/"))
        url.append("/");
    if (*path != 0 && StartsWith(path, "/"))
        ++path;
    url.append(path);
}

void ComposeUrl(std::string& url, Protocol proto,
                const char* host, const char* path)
{
    const char* prefix = CurlEasy::GetProtocolPrefix(proto);
    if (!StartsWith(host, prefix))
        url.append(prefix);
    ComposeUrl(url, host, path);
}

bool EscapeUrl(std::string& url, const char* pUrl)
{
    char* pEscapedUrl = curl_escape(pUrl, 0);
    if (!pEscapedUrl)
        return false;
    url = std::string(pEscapedUrl);
    curl_free(pEscapedUrl);
    return true;
}
