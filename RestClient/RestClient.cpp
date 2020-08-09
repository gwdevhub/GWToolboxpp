#include "stdafx.h"

#include <Core/Thread.h>

#include "RestClient.h"

class CurlMultiThread : public Thread
{
    typedef std::deque<AsyncRestClient*> Container;
public:

    CurlMultiThread()
        : m_pMulti(nullptr)
    {
        SetThreadName("CurlMultiThread");
    }

    void Start()
    {
        m_Running = true;
        StartThread();

        // @Remark:
        // Not ideal, but we have to wait for 'm_pMulti' to be setted and it is in the thread.
        // Otherwise, there is cases where 'CurlMultiThread::Execute' block.
        while (!m_pMulti)
            Sleep(1);
    }

    void Stop()
    {
        m_Running = false;
        Join();
    }

    void Execute(AsyncRestClient* pClient)
    {
        std::lock_guard<std::recursive_mutex> Lock(m_Mutex);
        m_Clients.push_back(pClient);
        m_pMulti->AddHandle(pClient);
    }

    void Abort(AsyncRestClient* pClient)
    {
        std::lock_guard<std::recursive_mutex> Lock(m_Mutex);
        Container::iterator it = Search(pClient);
        if (it != m_Clients.end())
        {
            m_pMulti->RemoveHandle(pClient);
            m_Clients.erase(it);
        }
    }

private:
    void Run() override
    {
        CurlMulti m_Multi;
        m_pMulti = &m_Multi;

        while (m_Running)
        {
            {
                std::lock_guard<std::recursive_mutex> Lock(m_Mutex);
                m_Multi.Perform();

                int MsgsLeft;
                struct CURLMsg *pMsg = curl_multi_info_read(m_Multi.GetHandle(), &MsgsLeft);
                while (pMsg)
                {
                    AsyncRestClient* pClient = SearchPop(pMsg->easy_handle);
                    m_Multi.RemoveHandle(pClient);
                    pClient->OnCompletion(pMsg->data.result);

                    pMsg = curl_multi_info_read(m_Multi.GetHandle(), &MsgsLeft);
                }
            }

            Sleep(16);
        }

        m_pMulti = nullptr;
    }

    Container::iterator Search(CURL* pHandle)
    {
        Container::iterator it;
        for (it = m_Clients.begin(); it != m_Clients.end(); ++it)
        {
            if ((*it)->GetHandle() == pHandle)
                break;
        }
        return it;
    }

    Container::iterator Search(AsyncRestClient* pClient)
    {
        Container::iterator it;
        for (it = m_Clients.begin(); it != m_Clients.end(); ++it)
        {
            if (*it == pClient)
                break;
        }
        return it;
    }

    AsyncRestClient* SearchPop(CURL* pHandle)
    {
        Container::iterator it = Search(pHandle);
        if (it == m_Clients.end())
            return nullptr;
        AsyncRestClient* pClient = *it;
        m_Clients.erase(it);
        return pClient;
    }

    Container m_Clients;
    CurlMulti* m_pMulti;
    std::atomic<bool> m_Running;
    std::recursive_mutex m_Mutex;
};

static CurlMultiThread s_RestThread;
static std::atomic<int> s_InitializeCount;
void InitAsyncRest()
{
    if (++s_InitializeCount == 1)
    {
        InitCurl();
        s_RestThread.Start();
    }
}

void ShutdownAsyncRest()
{
    assert(s_InitializeCount > 0);
    if (--s_InitializeCount == 0)
    {
        s_RestThread.Stop();
        ShutdownCurl();
    }
}

RestClient::RestClient()
{
}

void RestClient::Execute()
{
    Perform();
}

AsyncRestClient::AsyncRestClient()
    : m_Event(true, true)
{
}

AsyncRestClient::~AsyncRestClient()
{
    assert(!IsPending());
}

void AsyncRestClient::Clear()
{
    assert(!IsPending());
    RestClient::Clear();
}

void AsyncRestClient::Wait()
{
    m_Event.WaitUntilDone();
}

bool AsyncRestClient::Wait(uint32_t TimeoutMs)
{
    return m_Event.WaitWithTimeout(TimeoutMs);
}

bool AsyncRestClient::IsPending()
{
    return !Wait(0);
}

bool AsyncRestClient::IsCompleted()
{
    return !IsPending() && (m_Status != ResponseStatus::None);
}

void AsyncRestClient::ExecuteAsync()
{
    Clear();
    m_Event.Reset();
    s_RestThread.Execute(this);
}

void AsyncRestClient::Abort()
{
    if (IsPending())
    {
        s_RestThread.Abort(this);
        m_Event.SetDone();
    }
}

void AsyncRestClient::OnCompletion(int Status)
{
    UpdateStatus(Status);
    OnPerformed();
    m_Event.SetDone();
}
