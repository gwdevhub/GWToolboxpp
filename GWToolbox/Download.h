#pragma once

#include "Window.h"

bool Download(std::string& content, const wchar_t *url);
bool Download(const wchar_t *path_to_file, const wchar_t *url);

class DownloadWindow : public Window
{
public:
    static bool DownloadAllFiles();

public:
    DownloadWindow();
    ~DownloadWindow();

    bool Create();

private:
    LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    void OnCreate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hProgressBar;
};
