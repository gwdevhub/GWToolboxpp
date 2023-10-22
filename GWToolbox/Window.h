#pragma once

class Window {
    static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
    Window();
    Window(const Window&) = delete;
    Window(Window&&) = delete;
    virtual ~Window();

    Window& operator=(const Window&) = delete;
    Window& operator=(Window&&) = delete;

    virtual bool Create();
    bool WaitMessages() const;
    bool PollMessages(uint32_t TimeoutMs) const;
    static bool ProcessMessages();

    void SignalStop() const;
    bool ShouldClose() const;

    void SetWindowName(LPCWSTR lpName);
    void SetWindowPosition(int X, int Y);
    void SetWindowDimension(int Width, int Height);

    // void SetMaximizeButton(bool Enable);
    // void SetMinimizeButton(bool Enable);

private:
    virtual LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;

protected:
    HWND m_hWnd;
    HFONT m_hFont;
    HICON m_hIcon;
    HINSTANCE m_hInstance;

private:
    HANDLE m_hEvent;

    LPCWSTR m_WindowName;
    int m_X;
    int m_Y;
    int m_Width;
    int m_Height;
};
