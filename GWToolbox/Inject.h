#pragma once

#include "Process.h"
#include "Settings.h"
#include "Window.h"

enum InjectReply {
    InjectReply_Inject,
    InjectReply_Cancel,
    InjectReply_NoProcess,
    InjectReply_PatternError,
    InjectReply_NoValidProcess
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

    bool Create() override;

    // Returns false if no options were selected, typically when the window was closed.
    bool GetSelected(size_t* index) const;

private:
    LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    void OnCreate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void OnCommand(HWND hwnd, LONG ControlId, LONG NotificateCode);

private:
    HWND m_hCharacters;
    HWND m_hLaunchButton;
    HWND m_hRestartAsAdmin;
    HWND m_hSettings;
    SettingsWindow m_SettingsWindow;

    int m_Selected;
};

bool InjectRemoteThread(const Process* process, LPCWSTR ImagePath, LPDWORD lpExitCode);
