#pragma once

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#ifdef NOMINMAX
# define NOMINMAX
#endif
#include <Windows.h>

#include <stdint.h>

#include <string>
#include <vector>

class ProcessModule
{
public:
    ProcessModule() = default;
    ProcessModule(const ProcessModule&) = default;
    ProcessModule(ProcessModule&& other);

public:
    uintptr_t base = 0;
    size_t size = 0;
    std::wstring name;
};

class Process
{
public:
    Process();
    Process(uint32_t pid, DWORD rights = PROCESS_ALL_ACCESS);
    Process(const Process&) = delete;
    Process(Process&& other);
    ~Process();

    Process& operator=(const Process&) = delete;
    Process& operator=(Process&&);

    bool IsOpen();
    bool Open(uint32_t pid, DWORD rights = PROCESS_ALL_ACCESS);
    void Close();

    bool Read(uintptr_t address, void *buffer, size_t size);
    bool Write(uintptr_t address, void *buffer, size_t size);

    bool GetName(std::wstring& name);
    bool GetModule(ProcessModule *module);
    bool GetModule(ProcessModule *module, const wchar_t *module_name);
    bool GetModules(std::vector<ProcessModule>& modules);

    HANDLE GetHandle() { return m_hProcess; }
    uint32_t GetProcessId();
private:
    HANDLE m_hProcess;
};

bool GetProcesses(std::vector<Process>& processes, const wchar_t *name, DWORD rights = PROCESS_ALL_ACCESS);
bool GetProcessesFromWindowClass(std::vector<Process>& processes, const wchar_t *classname, DWORD rights = PROCESS_ALL_ACCESS);

class ProcessScanner
{
public:
    ProcessScanner(Process *process);
    ProcessScanner(const ProcessScanner&) = delete;
    ~ProcessScanner();

    uintptr_t FindPattern(const char *pattern, const char *mask, int Offset);
    bool FindPatternRva(const char *pattern, const char *mask, int offset, uintptr_t *rva);

private:
    uintptr_t m_base;
    size_t m_size;
    uint8_t *m_buffer;
};
