#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Process.h"
#include "WasmInject.h"

struct InjectProcess {
    InjectProcess(const bool injected, Process&& process, std::wstring&& charname) : m_Injected(injected), m_Process(std::move(process)), m_Charname(std::move(charname)) {}
    InjectProcess(const bool injected, WasmSession&& session, std::wstring&& label) : m_Injected(injected), m_WasmSession(std::move(session)), m_Charname(std::move(label)) {}

    InjectProcess(const InjectProcess&) = delete;
    InjectProcess(InjectProcess&&) = default;

    InjectProcess& operator=(const InjectProcess&) = delete;
    InjectProcess& operator=(InjectProcess&&) = default;

    bool IsWasm() const { return m_WasmSession.has_value(); }

    bool m_Injected;
    Process m_Process;
    std::optional<WasmSession> m_WasmSession;
    std::wstring m_Charname;
};

struct InjectScanResult {
    bool m_Found = false;
    std::vector<InjectProcess> m_Processes;

    std::wstring m_ErrorMessage;
    const wchar_t* m_TroubleshootingUrl = nullptr;
};

bool InjectRemoteThread(const Process* process, LPCWSTR ImagePath, LPDWORD lpExitCode);
bool InjectInstalledDllInProcess(const Process* process, std::wstring& error);
bool InjectWasmModIntoSession(const WasmSession& session, std::wstring& error);
bool SetProcessForeground(const Process* process);

std::vector<Process> GetGuildWarsProcesses();
std::vector<Process> GetGuildWars2Processes();
std::vector<std::filesystem::path> GetGuildWarsExecutablePaths();

InjectScanResult RunScan();
