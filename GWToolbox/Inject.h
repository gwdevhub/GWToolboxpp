#pragma once

#include <memory>
#include <optional>

#include "Download.h"
#include "LauncherWindow.h"
#include "Process.h"
#include "Settings.h"
#include "WasmInject.h"

enum InjectReply {
    InjectReply_Inject,
    InjectReply_Cancel,
};

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

struct InjectSelection {
    bool is_wasm = false;
    Process process;
    std::optional<WasmSession> wasm_session;
};

struct InjectScanResult {
    bool m_Found = false;
    std::vector<InjectProcess> m_Processes;

    std::wstring m_ErrorMessage;
    const wchar_t* m_TroubleshootingUrl = nullptr;
};

class InjectWindow : public LauncherWindow {
public:
    static InjectReply AskInjectProcess(InjectSelection* selection);

    explicit InjectWindow(InjectScanResult scan);
    InjectWindow(const InjectWindow&) = delete;
    InjectWindow(InjectWindow&&) = delete;
    ~InjectWindow() override = default;

    InjectWindow& operator=(const InjectWindow&) = delete;
    InjectWindow& operator=(InjectWindow&&) = delete;

    bool Finished() const;

    bool GetSelected(size_t* index) const;
    InjectSelection TakeSelected(size_t index);

protected:
    void onClosed() override;

private:
    void RegisterMethods();
    Json State() const;
    void ResetScan(InjectScanResult scan);
    void OpenSettings();

    InjectScanResult m_ScanResult;
    size_t m_Preselected = 0;
    std::optional<size_t> m_Selected;
    UpdateChecker m_UpdateChecker;
    std::unique_ptr<SettingsWindow> m_SettingsWindow;
};

bool InjectRemoteThread(const Process* process, LPCWSTR ImagePath, LPDWORD lpExitCode);

std::vector<Process> GetGuildWarsProcesses();
std::vector<Process> GetGuildWars2Processes();

std::vector<std::filesystem::path> GetGuildWarsExecutablePaths();
