#pragma once

#include <Event.h>

#include "CurlWrapper.h"

void InitAsyncRest();
void ShutdownAsyncRest();

class AsyncRestScopeInit
{
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

class RestClient : public CurlEasy
{
public:
    RestClient();
    RestClient(const RestClient&) = delete;
    RestClient& operator=(const RestClient&) = delete;

    void Execute();
    bool IsSuccessful()
    {
        if (m_StatusCode < 200 || 300 <= m_StatusCode)
            return false;
        return (m_Status == ResponseStatus::Completed);
    }
};

class AsyncRestClient : public RestClient
{
public:
    AsyncRestClient();
    AsyncRestClient(const AsyncRestClient&) = delete;
    AsyncRestClient& operator=(const AsyncRestClient&) = delete;

    ~AsyncRestClient();

    // Clears the data from the request which effectively set the request to "incomplete".
    // So "IsCompleted" will return false.
    void Clear();

    // @Remark:
    // The functions from RestClient (Recursively from CurlEasy) are not
    // thread-safe, but the api "Wait", "IsPending" and "Abort" are thread-safe.
    // Typically, the setters from CurlEasy are not thread-safe.
    //
    // You need to guarantee that between a call to "ExecuteAsync" until
    // "IsPending" returns false (or "Wait" returns true) or until "Abort"
    //  is called the options are not changed.
    void Wait();
    bool Wait(uint32_t TimeoutMs);

    // "IsPending" returns true as long as the request is potentially used in an other thread.
    bool IsPending();

    // "IsCompleted" returns true if the asynchronous operation is finished.
    // This flag is reseted when "Clear" is called.
    bool IsCompleted();

    void ExecuteAsync();
    void Abort();

    void OnCompletion(int Status);

private:
    Event m_Event;
};
