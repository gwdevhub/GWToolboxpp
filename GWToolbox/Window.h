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
    // Scales a design-time pixel value (authored against 96 DPI) to the monitor DPI recorded by ApplyDpiScaling.
    int Scale(int value) const;

    // Call as the first line of a derived OnCreate: records the window's actual monitor DPI and resizes it to match, so Scale() can lay out children correctly.
    void ApplyDpiScaling(HWND hWnd);

    // Call from a derived WndProc's WM_DPICHANGED case to keep the top-level window sized/positioned correctly if dragged to a different-DPI monitor.
    void OnDpiChanged(WPARAM wParam, LPARAM lParam);

    HWND m_hWnd;
    HFONT m_hFont;
    HICON m_hIcon;
    HINSTANCE m_hInstance;
    int m_Dpi;
    int m_Width;
    int m_Height;

private:
    HANDLE m_hEvent;

    LPCWSTR m_WindowName;
    int m_X;
    int m_Y;
};
