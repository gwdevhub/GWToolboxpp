#pragma once

#include "Window.h"

bool Download(std::string& content, const wchar_t *url);
bool Download(const wchar_t *path_to_file, const wchar_t *url);

class DownloadWindow : public Window
{
public:
    DownloadWindow() = default;
    DownloadWindow(const DownloadWindow&) = delete;
    ~DownloadWindow() override = default;

    DownloadWindow& operator=(const DownloadWindow&) = delete;

    bool Create();
    static bool DownloadAllFiles();
    void SetChangelog(const char *str, size_t length) const;

private:
    LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    void OnCreate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hProgressBar = nullptr;
    HWND m_hCloseButton = nullptr;
    HWND m_hChangelog = nullptr;
};
