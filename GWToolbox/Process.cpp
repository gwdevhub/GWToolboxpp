#include "stdafx.h"

#include "Process.h"

ProcessModule::ProcessModule(ProcessModule&& other)
    : size(other.size)
    , base(other.base)
    , name(std::move(other.name))
{
}

Process::Process()
    : m_hProcess(nullptr)
{
}

Process::Process(uint32_t pid, DWORD rights)
    : Process()
{
    Open(pid, rights);
}

Process::Process(Process&& other)
    : m_hProcess(other.m_hProcess)
{
    other.m_hProcess = nullptr;
}

Process::~Process()
{
    Close();
}

Process& Process::operator=(Process&& other)
{
    Close();
    m_hProcess = other.m_hProcess;
    other.m_hProcess = nullptr;
    return *this;
}

bool Process::IsOpen()
{
    return m_hProcess != nullptr;
}

bool Process::Open(uint32_t pid, DWORD rights)
{
    assert(rights & PROCESS_VM_READ);
    assert(rights & PROCESS_QUERY_INFORMATION);
    assert(rights & PROCESS_QUERY_LIMITED_INFORMATION);

    Close();
    m_hProcess = OpenProcess(rights, FALSE, pid);
    if (m_hProcess == nullptr) {
        // fprintf(stderr, "OpenProcess failed: %lu\n", GetLastError());
        return false;
    }

    return true;
}

void Process::Close()
{
    if (m_hProcess) {
        CloseHandle(m_hProcess);
        m_hProcess = nullptr;
    }
}

bool Process::Read(uintptr_t address, void *buffer, size_t size)
{
    SIZE_T NumberOfBytesRead;
    LPCVOID BaseAddress = reinterpret_cast<LPCVOID>(address);
    BOOL success = ReadProcessMemory(
        m_hProcess,
        BaseAddress,
        buffer,
        size,
        &NumberOfBytesRead);

    if (!(success && size == NumberOfBytesRead)) {
        fprintf(stderr, "ReadProcessMemory failed: %lu\n", GetLastError());
        return false;
    }

    return true;
}

bool Process::Write(uintptr_t address, void *buffer, size_t size)
{
    SIZE_T NumberOfBytesWritten;
    LPVOID BaseAddress = reinterpret_cast<LPVOID>(address);
    BOOL success = WriteProcessMemory(
        m_hProcess,
        BaseAddress,
        buffer,
        size,
        &NumberOfBytesWritten);

    if (!(success && size == NumberOfBytesWritten)) {
        fprintf(stderr, "WriteProcessMemory failed: %lu\n", GetLastError());
        return false;
    }

    return true;
}

bool Process::GetName(std::wstring& name)
{
    std::wstring process_path;
    process_path.resize(1024);
    for (;;) {
        DWORD size = process_path.size();
        if (!QueryFullProcessImageNameW(m_hProcess, 0, &process_path[0], &size)) {
            DWORD error = GetLastError();
            if (error != ERROR_INSUFFICIENT_BUFFER) {
                fprintf(stderr, "QueryFullProcessImageNameW failed: %lu\n", error);
                Close();
                return false;
            }
            size_t new_size = process_path.size() * 2;
            process_path.resize(new_size);
        } else {
            process_path.resize(size);
            break;
        }
    }

    std::string::size_type pos = process_path.rfind('\\');
    if (pos == std::string::npos) {
        fprintf(stderr, "Invalid process path: '%ls'\n", process_path.c_str());
        Close();
        return false;
    }

    name = process_path.substr(pos + 1);
    return true;
}

bool Process::GetModule(ProcessModule *module)
{
    std::wstring pname;
    if (!GetName(pname))
        return false;
    return GetModule(module, pname.c_str());
}

bool Process::GetModule(ProcessModule *module, const wchar_t *module_name)
{
    DWORD cbNeeded;
    if (!EnumProcessModules(m_hProcess, nullptr, 0, &cbNeeded)) {
        fprintf(stderr, "EnumProcessModules failed: %lu\n", GetLastError());
        return false;
    }

    std::vector<HMODULE> handles(cbNeeded / sizeof(HMODULE));
    if (!EnumProcessModules(m_hProcess, handles.data(), cbNeeded, &cbNeeded)) {
        fprintf(stderr, "EnumProcessModules failed: %lu\n", GetLastError());
        return false;
    }

    for (HMODULE hModule : handles) {
        wchar_t name[512];
        if (!GetModuleBaseNameW(m_hProcess, hModule, name, sizeof(name))) {
            fprintf(stderr, "GetModuleBaseNameW failed: %lu\n", GetLastError());
            return false;
        }

        MODULEINFO ModuleInfo;
        if (!GetModuleInformation(m_hProcess, hModule, &ModuleInfo, sizeof(ModuleInfo))) {
            fprintf(stderr, "GetModuleInformation failed: %lu\n", GetLastError());
            return false;
        }

        // @Cleanup: Replace by case insensitive compare
        if (!wcscmp(name, module_name)) {
            module->base = reinterpret_cast<uintptr_t>(ModuleInfo.lpBaseOfDll);
            module->size = ModuleInfo.SizeOfImage;
            module->name = name;
            return true;
        }
    }

    return false;
}

bool Process::GetModules(std::vector<ProcessModule>& modules)
{
    DWORD cbNeeded;
    if (!EnumProcessModules(m_hProcess, nullptr, 0, &cbNeeded)) {
        fprintf(stderr, "EnumProcessModules failed: %lu\n", GetLastError());
        return false;
    }

    std::vector<HMODULE> handles(cbNeeded / sizeof(HMODULE));
    if (!EnumProcessModules(m_hProcess, handles.data(), cbNeeded, &cbNeeded)) {
        fprintf(stderr, "EnumProcessModules failed: %lu\n", GetLastError());
        return false;
    }

    for (HMODULE hModule : handles) {
        wchar_t name[512];
        if (!GetModuleBaseNameW(m_hProcess, hModule, name, sizeof(name))) {
            fprintf(stderr, "GetModuleBaseNameW failed: %lu\n", GetLastError());
            return false;
        }

        MODULEINFO ModuleInfo;
        if (!GetModuleInformation(m_hProcess, hModule, &ModuleInfo, sizeof(ModuleInfo))) {
            fprintf(stderr, "GetModuleInformation failed: %lu\n", GetLastError());
            return false;
        }

        ProcessModule module;
        module.base = reinterpret_cast<uintptr_t>(ModuleInfo.lpBaseOfDll);
        module.size = ModuleInfo.SizeOfImage;
        module.name = name;
        modules.push_back(std::move(module));
    }

    return true;
}

uint32_t Process::GetProcessId()
{
    return ::GetProcessId(m_hProcess);
}

bool GetProcesses(std::vector<Process>& processes, const wchar_t *name, DWORD rights)
{
    assert(name != nullptr);

    std::vector<DWORD> pids(1024);
    for (;;) {
        DWORD size = pids.size() * sizeof(DWORD);
        DWORD bytes;
        if (!EnumProcesses(pids.data(), size, &bytes)) {
            fprintf(stderr, "EnumProcesses failed: %lu\n", GetLastError());
            return false;
        }

        if (bytes < size) {
            size_t count = bytes / sizeof(DWORD);
            pids.resize(count);
            break;
        } else {
            size_t new_size = pids.size() * 2;
            pids.resize(new_size);
        }
    }

    for (DWORD pid : pids) {
        Process proc(pid, rights);
        std::wstring pname;
        if (!(proc.IsOpen() && proc.GetName(pname)))
            continue;
        // @Cleanup: Replace by case insensitive compare
        if (pname == name)
            processes.push_back(std::move(proc));
    }

    return true;
}

bool GetProcessesFromWindowClass(std::vector<Process>& processes, const wchar_t *classname, DWORD rights)
{
    // EnumWindows()
    return true;
}

ProcessScanner::ProcessScanner(Process *process)
    : m_size(0)
    , m_buffer(nullptr)
{
    ProcessModule module;
    process->GetModule(&module);

    m_base = module.base;
    m_size = module.size;
    m_buffer = new uint8_t[module.size];
    process->Read(module.base, m_buffer, m_size);
}

ProcessScanner::~ProcessScanner()
{
    if (m_buffer)
        delete[] m_buffer;
}

uintptr_t ProcessScanner::FindPattern(const char *pattern, const char *mask, int offset)
{
    uintptr_t rva;
    if (FindPatternRva(pattern, mask, offset, &rva)) {
        return m_base + rva;
    } else {
        return 0;
    }
}

bool ProcessScanner::FindPatternRva(const char *pattern, const char *mask, int offset, uintptr_t *rva)
{
    size_t length = strlen(mask);
    const uint8_t *upattern = reinterpret_cast<const uint8_t *>(pattern);
    for (size_t i = 0; i < m_size; i++) {
        size_t j;
        for (j = 0; j < length; j++) {
            if (mask[j] == 'x' && m_buffer[i + j] != upattern[j])
                break;
        }
        if (j == length) {
            *rva = i + offset;
            return true;
        }
    }
    return false;
}
