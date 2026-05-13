#include "stdafx.h"

#include <Thread.h>

#include "RestClient.h"

// ---------------------------------------------------------------------------
// CurlMultiThread — stub implementation (no libcurl).
// AsyncRestClient::ExecuteAsync() falls back to synchronous execution on a
// worker thread.  Resources.cpp never calls ExecuteAsync(), so this is fine.
// ---------------------------------------------------------------------------
class CurlMultiThread : public Thread {
    using Container = std::deque<AsyncRestClient*>;

public:
    CurlMultiThread() { SetThreadName("CurlMultiThread"); }

    void Start()
    {
        m_Running = true;
        StartThread();
        // Give the thread a moment to spin up
        while (!m_Started) {
            Sleep(1);
        }
    }

    void Stop()
    {
        m_Running = false;
        Join();
    }

    void Execute(AsyncRestClient* pClient)
    {
        std::lock_guard Lock(m_Mutex);
        m_Clients.push_back(pClient);
    }

    void Abort(AsyncRestClient* pClient)
    {
        std::lock_guard Lock(m_Mutex);
        const auto it = std::find(m_Clients.begin(), m_Clients.end(), pClient);
        if (it != m_Clients.end()) m_Clients.erase(it);
    }

private:
    void Run() override
    {
        m_Started = true;
        while (m_Running) {
            AsyncRestClient* pClient = nullptr;
            {
                std::lock_guard Lock(m_Mutex);
                if (!m_Clients.empty()) {
                    pClient = m_Clients.front();
                    m_Clients.pop_front();
                }
            }
            if (pClient) {
                const bool ok = pClient->Perform();
                pClient->OnCompletion(ok ? 0 : 1);
            }
            else {
                Sleep(16);
            }
        }
    }

    Container m_Clients;
    std::atomic<bool> m_Running{false};
    std::atomic<bool> m_Started{false};
    std::recursive_mutex m_Mutex;
};

static CurlMultiThread s_RestThread;
static std::atomic<int> s_InitializeCount;

void InitAsyncRest()
{
    if (++s_InitializeCount == 1) {
        InitCurl(); // no-op
        s_RestThread.Start();
    }
}

void ShutdownAsyncRest()
{
    assert(s_InitializeCount > 0);
    if (--s_InitializeCount == 0) {
        s_RestThread.Stop();
        ShutdownCurl(); // no-op
    }
}

RestClient::RestClient() {}

void RestClient::Execute()
{
    Perform();
}

AsyncRestClient::AsyncRestClient() : m_Event(true, true) {}

AsyncRestClient::~AsyncRestClient()
{
    assert(!IsPending());
}

void AsyncRestClient::Clear()
{
    assert(!IsPending());
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
    Clear();
    m_Event.Reset();
    s_RestThread.Execute(this);
}

void AsyncRestClient::Abort()
{
    if (IsPending()) {
        s_RestThread.Abort(this);
        m_Event.SetDone();
    }
}

void AsyncRestClient::OnCompletion(const int Status)
{
    UpdateStatus(Status);
    OnPerformed();
    m_Event.SetDone();
}
