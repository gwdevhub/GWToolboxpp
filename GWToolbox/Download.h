#pragma once

bool Download(std::string& content, const wchar_t *url);
bool Download(const wchar_t *path_to_file, const wchar_t *url);

class DownloadWindow
{
public:
    static bool DownloadAllFiles();

private:
    static void OnWindowCreate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
    DownloadWindow();
    ~DownloadWindow();

    bool Create();
    bool WaitMessages();

private:
    LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hWnd;
    HWND m_hProgressBar;
    HANDLE m_hEvent;
    HINSTANCE m_hInstance;
};
