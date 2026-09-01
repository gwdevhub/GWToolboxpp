#pragma once

#include <Event.h>

#include "HttpClient.h"

void InitAsyncRest();
void ShutdownAsyncRest();

class AsyncRestScopeInit {
public:
    AsyncRestScopeInit()
    {
        InitAsyncRest();
    }

    ~AsyncRestScopeInit()
    {
        ShutdownAsyncRest();
    }
};

class RestClient : public HttpRequest {
public:
    RestClient();
    RestClient(const RestClient&) = delete;
    RestClient& operator=(const RestClient&) = delete;

    void Execute();

    bool IsSuccessful() const
    {
        if (m_StatusCode < 200 || m_StatusCode > 302) {
            return false;
        }
        return m_Status == ResponseStatus::Completed;
    }
};

class AsyncRestClient : public RestClient {
public:
    AsyncRestClient();
    AsyncRestClient(const AsyncRestClient&) = delete;
    AsyncRestClient& operator=(const AsyncRestClient&) = delete;

    ~AsyncRestClient() override;

    // Clears the response data. Effectively sets the request to "incomplete"
    // so "IsCompleted" will return false.
    void Clear();

    // "Wait", "IsPending" and "Abort" are thread-safe; the inherited setters are not
    // and must not be touched between "ExecuteAsync" and the request finishing.
    void Wait() const;
    bool Wait(uint32_t TimeoutMs) const;

    // "IsPending" returns true as long as the request is potentially used in
    // another thread.
    bool IsPending();

    // "IsCompleted" returns true if the asynchronous operation is finished.
    // This flag is reseted when "Clear" is called.
    bool IsCompleted();

    void ExecuteAsync();
    void Abort();

private:
    static unsigned long __stdcall ThreadProc(void* param);
    void JoinThread();

    void* m_ThreadHandle;
    Event m_Event;
};
