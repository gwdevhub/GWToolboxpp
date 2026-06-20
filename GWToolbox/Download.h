#pragma once

#include "Window.h"

bool Download(std::string& content, const wchar_t* url);
bool Download(const wchar_t* path_to_file, const wchar_t* url);

struct Release;

class DownloadWindow : public Window {
public:
    DownloadWindow() = default;
    DownloadWindow(const DownloadWindow&) = delete;
    ~DownloadWindow() override = default;

    DownloadWindow& operator=(const DownloadWindow&) = delete;

    bool Create() override;
    // Single Github fetch feeding the exe self-update check then the dll update; replaces two separate API calls.
    static bool CheckForUpdates(std::wstring& error);
    // dll-only update, fetching its own release list; used by the installer.
    static bool DownloadDll(std::wstring& error);
    void SetChangelog(const char* str, size_t length) const;

private:
    static bool DownloadDll(const std::vector<Release>& releases, std::wstring& error);

    LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    void OnCreate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    HWND m_hProgressBar = nullptr;
    HWND m_hCloseButton = nullptr;
    HWND m_hChangelog = nullptr;
    HWND m_hStatusLabel = nullptr;
};
