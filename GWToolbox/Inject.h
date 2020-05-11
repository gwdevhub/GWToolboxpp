#pragma once

#include "Process.h"

class InjectWindow
{
public:
    static bool AskInjectProcess(Process *process);

private:
    static void OnWindowCreate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
    InjectWindow();
    InjectWindow(const InjectWindow&) = delete;
    InjectWindow(InjectWindow&&) = delete;
    ~InjectWindow();

    bool Create(const wchar_t *title, std::vector<std::wstring>& names);
    bool WaitMessages();

private:
    void OnEvent(HWND hwnd, LONG control_id, LONG notification_code);

private:
    HWND m_hWnd;
    HWND m_hCharacters;
    HWND m_hInjectButton;
    HANDLE m_hEvent;
    HINSTANCE m_hInstance;

    int m_Selected;
};

bool IsRunningAsAdmin();
bool CreateProcessAsAdmin(const wchar_t *path, const wchar_t *args, const wchar_t *workdir);
bool RestartAsAdmin(const wchar_t *args);
bool EnableDebugPrivilege();
bool InjectRemoteThread(Process *process, LPCWSTR ImagePath, LPDWORD lpExitCode);
