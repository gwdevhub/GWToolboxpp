#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <string>
#include <vector>

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
    HWND m_window;
    HWND m_characters_combo;
    HWND m_inject_button;
    HANDLE m_event;
    HINSTANCE m_instance;

    int m_selected;
};

bool IsRunningAsAdmin();
bool CreateProcessAsAdmin(const wchar_t *path, const wchar_t *args, const wchar_t *workdir);
bool RestartAsAdmin(const wchar_t *args = L"");
bool EnableDebugPrivilege();

bool InjectRemoteThread(Process& process, LPCWSTR ImagePath, LPDWORD lpExitCode);
