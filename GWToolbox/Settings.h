#pragma once

#include "Window.h"

struct Settings
{
    bool help;
    bool version;
    bool quiet;
    bool install;
    bool uninstall;
    bool reinstall;
    bool asadmin;
    bool noupdate;
    int  pid;
};

extern Settings settings;

void PrintUsage(bool terminate);

void ParseRegSettings();
void ParseCommandLine();

bool IsRunningAsAdmin();
bool CreateProcessAsAdmin(const wchar_t *path, const wchar_t *args, const wchar_t *workdir);
bool RestartAsAdmin(const wchar_t *args);
void RestartAsAdminWithSameArgs();
bool EnableDebugPrivilege();

class SettingsWindow : public Window
{
public:
    SettingsWindow();
    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow(SettingsWindow&&) = delete;
    ~SettingsWindow();

    bool Create();

private:
    LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    void OnCreate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void OnCommand(HWND hwnd, LONG ControlId, LONG NotificateCode);

private:
    HWND m_hNoUpdate;
    HWND m_hStartAsAdmin;
};
