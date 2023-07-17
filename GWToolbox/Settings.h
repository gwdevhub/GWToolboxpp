#pragma once

#include "Window.h"

struct Settings
{
    bool help = false;
    bool version = false;
    bool quiet = false;
    bool install = false;
    bool uninstall = false;
    bool reinstall = false;
    bool asadmin = false;
    bool noupdate = false;
    bool noinstall = false;
    bool localdll = false;
    uint32_t pid;
};

extern Settings settings;

void PrintUsage(bool terminate);

void ParseRegSettings();
void ParseCommandLine();

bool IsRunningAsAdmin();
bool CreateProcessInt(const wchar_t *path, const wchar_t *args, const wchar_t *workdir, bool as_admin = false);
bool Restart(const wchar_t* args, bool force_admin = false);
bool RestartAsAdmin(const wchar_t* args);
void RestartWithSameArgs(bool force_admin = false);
bool EnableDebugPrivilege();

class SettingsWindow : public Window
{
public:
    SettingsWindow() = default;
    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow(SettingsWindow&&) = delete;
    ~SettingsWindow() override = default;

    SettingsWindow& operator=(const SettingsWindow&) = delete;

    bool Create();

private:
    LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    void OnCreate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void OnCommand(HWND hwnd, LONG ControlId, LONG NotificateCode);

private:
    HWND m_hNoUpdate = nullptr;
    HWND m_hStartAsAdmin = nullptr;
};
