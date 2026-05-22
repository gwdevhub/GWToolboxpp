#include "stdafx.h"

#include <windows.h>

#include "RestClient.h"

static std::atomic<int> s_InitializeCount;

void InitAsyncRest()
{
    if (++s_InitializeCount == 1) {
        InitHttpClient();
    }
}

void ShutdownAsyncRest()
{
    assert(s_InitializeCount > 0);
    if (--s_InitializeCount == 0) {
        ShutdownHttpClient();
    }
}

RestClient::RestClient() {}

void RestClient::Execute()
{
    Perform();
}

AsyncRestClient::AsyncRestClient()
    : m_ThreadHandle(nullptr)
    , m_Event(true, true) {}

AsyncRestClient::~AsyncRestClient()
{
    assert(!IsPending());
    JoinThread();
}

void AsyncRestClient::Clear()
{
    assert(!IsPending());
    JoinThread();
    RestClient::Clear();
}

void AsyncRestClient::Wait() const
{
    m_Event.WaitUntilDone();
}

bool AsyncRestClient::Wait(const uint32_t TimeoutMs) const
{
    return m_Event.WaitWithTimeout(TimeoutMs);
}

bool AsyncRestClient::IsPending()
{
    return !Wait(0);
}

bool AsyncRestClient::IsCompleted()
{
    return !IsPending() && m_Status != ResponseStatus::None;
}

void AsyncRestClient::ExecuteAsync()
{
    // Synchronously join any previous async run so we can safely reuse this
    // instance.
    JoinThread();
    RestClient::Clear();
    m_Event.Reset();
    m_ThreadHandle = CreateThread(nullptr, 0, &AsyncRestClient::ThreadProc, this, 0, nullptr);
    if (!m_ThreadHandle) {
        // Failed to spawn a worker; treat as immediately errored.
        m_Status = ResponseStatus::Error;
        m_Event.SetDone();
    }
}

void AsyncRestClient::Abort()
{
    if (IsPending()) {
        RequestAbort();
        m_Event.WaitUntilDone();
    }
    JoinThread();
}

unsigned long __stdcall AsyncRestClient::ThreadProc(void* param)
{
    auto* self = static_cast<AsyncRestClient*>(param);
    self->Perform();
    self->m_Event.SetDone();
    return 0;
}

void AsyncRestClient::JoinThread()
{
    if (m_ThreadHandle) {
        WaitForSingleObject(static_cast<HANDLE>(m_ThreadHandle), INFINITE);
        CloseHandle(static_cast<HANDLE>(m_ThreadHandle));
        m_ThreadHandle = nullptr;
    }
}
