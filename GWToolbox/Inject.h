#pragma once

#include <optional>

#include "Download.h"
#include "Process.h"
#include "Settings.h"
#include "Window.h"

enum InjectReply {
    InjectReply_Inject,
    InjectReply_Cancel,
};

// A running Guild Wars process paired with the character name (or email, if no character is selected yet) read from its memory.
struct InjectProcess {
    InjectProcess(const bool injected, Process&& process, std::wstring&& charname) : m_Injected(injected), m_Process(std::move(process)), m_Charname(std::move(charname)) {}

    InjectProcess(const InjectProcess&) = delete;
    InjectProcess(InjectProcess&&) = default;

    InjectProcess& operator=(const InjectProcess&) = delete;
    InjectProcess& operator=(InjectProcess&&) = default;

    bool m_Injected;
    Process m_Process;
    std::wstring m_Charname;
};

// Result of scanning for injectable Guild Wars processes: either a populated, ready-to-show character list, or an error to display in place of it.
struct InjectScanResult {
    bool m_Found = false;
    std::vector<InjectProcess> m_Processes; // valid, sorted by name, when m_Found

    std::wstring m_ErrorMessage; // display text when !m_Found
    const wchar_t* m_TroubleshootingUrl = nullptr; // non-null when the error box should offer a troubleshooting link
};

class InjectWindow : public Window {
public:
    static InjectReply AskInjectProcess(Process* target_process);

public:
    InjectWindow();
    InjectWindow(const InjectWindow&) = delete;
    InjectWindow(InjectWindow&&) = delete;
    ~InjectWindow() override;

    InjectWindow& operator=(const InjectWindow&) = delete;

    // Skips the initial scan in Create(), reusing a scan already performed by the caller (e.g. the quiet-mode check in AskInjectProcess).
    void SetInitialScanResult(InjectScanResult result);

    bool Create() override;

    // Returns false if no character was selected, typically when the window was closed.
    bool GetSelected(size_t* index) const;
    Process TakeSelectedProcess(size_t index);

private:
    LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    void OnCreate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void OnCommand(HWND hwnd, LONG ControlId, LONG NotificateCode);

    // (Re)builds every child control to match m_ScanResult, tearing down whatever was there before; used on first creation and after every Retry.
    void BuildControls(HWND hWnd);
    void Rescan();

    // Advances the background update check and, on the tick it resolves, refreshes the status row. Call every message-loop tick.
    void Tick();
    // Syncs the status row's text/button to the current UpdateChecker state; safe to call whether or not the row has been built yet.
    void RefreshUpdateStatus();
    void SetControlsEnabled(bool enabled);

private:
    HWND m_hCharacters;
    HWND m_hLaunchButton;
    HWND m_hErrorText;
    HWND m_hRetryButton;
    HWND m_hTroubleshootingLink;
    HWND m_hRestartAsAdmin;
    HWND m_hSettings;
    HWND m_hUpdateStatus;
    HWND m_hUpdateButton;
    HBRUSH m_hErrorBrush;
    SettingsWindow m_SettingsWindow;
    UpdateChecker m_UpdateChecker;

    InjectScanResult m_ScanResult;
    std::optional<InjectScanResult> m_PendingScanResult;

    int m_Selected;
};

bool InjectRemoteThread(const Process* process, LPCWSTR ImagePath, LPDWORD lpExitCode);

std::vector<Process> GetGuildWarsProcesses();
std::vector<Process> GetGuildWars2Processes();

// Full image paths of the currently running Guild Wars (Gw.exe) processes, de-duplicated.
std::vector<std::filesystem::path> GetGuildWarsExecutablePaths();
