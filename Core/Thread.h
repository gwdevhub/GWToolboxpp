#pragma once

class Thread {
public:
    Thread();
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    virtual ~Thread();

    bool StartThread();

    void Join();
    bool Join(uint32_t TimeoutMs) const;

    bool Alive() const;

    uint32_t GetExitCode() const;
    void SetThreadName(const char* name);

private:
    void* m_Handle;
    uint32_t m_ThreadId;
    char m_ThreadName[256];

private:
    virtual void Run() = 0;

    static unsigned long __stdcall ThreadEntry(void* param);
    __declspec(noreturn) static void Exit(uint32_t ErrorCode);
};
