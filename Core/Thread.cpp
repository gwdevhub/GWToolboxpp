#include "stdafx.h"

#include "Thread.h"

#pragma pack(push, 8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType;     // Must be 0x1000.
    LPCSTR szName;    // Pointer to name (in user addr space).
    DWORD dwThreadId; // Thread Id (-1=caller thread).
    DWORD dwFlags;    // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

static void SetThreadName(DWORD dwThreadId, LPCSTR szName)
{
    const DWORD MS_VC_EXCEPTION = 0x406D1388;

    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = szName;
    info.dwThreadId = dwThreadId;
    info.dwFlags = 0;

    __try {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

Thread::Thread()
    : m_Handle(nullptr)
    , m_ThreadId(0)
    , m_ThreadName("")
{
}

Thread::~Thread()
{
    assert(!m_Handle || !Alive());
    if (m_Handle != nullptr)
        CloseHandle(m_Handle);
}

bool Thread::StartThread()
{
    assert(!m_Handle);

    DWORD ThreadId;
    m_Handle = CreateThread(
        nullptr,
        0,
        Thread::ThreadEntry,
        this,
        0,
        &ThreadId);

    if (!m_Handle) {
        // @Enhancement: Add logging
        return false;
    }

    ::SetThreadName(ThreadId, m_ThreadName);
    m_ThreadId = ThreadId;
    return true;
}

void Thread::Join()
{
    Join(INFINITE);
}

bool Thread::Join(uint32_t TimeoutMs)
{
    assert(m_Handle);
    DWORD dwReason = WaitForSingleObject(m_Handle, TimeoutMs);
    return dwReason != WAIT_OBJECT_0;
}

bool Thread::Alive() const
{
    assert(m_Handle);
    DWORD dwReason = WaitForSingleObject(m_Handle, 0);
    return dwReason == WAIT_TIMEOUT;
}

uint32_t Thread::GetExitCode() const
{
    assert(!Alive());
    DWORD ExitCode;
    if (!GetExitCodeThread(m_Handle, &ExitCode)) {
        // @Enhancement: Add logging
        return 0;
    }
    return ExitCode;
}

void Thread::SetThreadName(const char* ThreadName)
{
    assert(!m_Handle);
    strcpy_s(m_ThreadName, sizeof(m_ThreadName), ThreadName);
}

unsigned long Thread::ThreadEntry(void* pParam)
{
    Thread* pThread = static_cast<Thread*>(pParam);
    pThread->Run();
    Thread::Exit(0);
}

void Thread::Exit(uint32_t ErrorCode)
{
    ExitThread(ErrorCode);
}
