#include "stdafx.h"

#include "Window.h"

namespace {
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);

    // GetDpiForWindow is Windows 10 1607+; resolve it dynamically so older Windows still starts, just without per-monitor scaling.
    GetDpiForWindowFn ResolveGetDpiForWindow()
    {
        const HMODULE user32 = GetModuleHandleW(L"user32.dll");
        return user32 ? reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow")) : nullptr;
    }
}

LRESULT CALLBACK Window::MainWndProc(HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    Window* window;

    if (uMsg == WM_CREATE) {
        const auto param = reinterpret_cast<LPCREATESTRUCTW>(lParam);
        window = static_cast<Window*>(param->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG>(window));
    }
    else {
        const LONG_PTR ud = GetWindowLongPtrW(hWnd, GWLP_USERDATA);
        window = reinterpret_cast<Window*>(ud);
    }

    if (window != nullptr) {
        return window->WndProc(hWnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

Window::Window()
    : m_hWnd(nullptr)
    , m_hFont(nullptr)
    , m_hIcon(nullptr)
    , m_hInstance(nullptr)
    , m_Dpi(USER_DEFAULT_SCREEN_DPI)
    , m_Width(CW_USEDEFAULT)
    , m_Height(CW_USEDEFAULT)
    , m_hEvent(nullptr)
    , m_WindowName(nullptr)
    , m_X(CW_USEDEFAULT)
    , m_Y(CW_USEDEFAULT)
{
    m_hInstance = GetModuleHandleW(nullptr);
}

Window::~Window()
{
    if (m_hEvent) {
        CloseHandle(m_hEvent);
    }
    if (m_hFont) {
        DeleteObject(m_hFont);
    }
}

int Window::Scale(const int value) const
{
    return MulDiv(value, m_Dpi, USER_DEFAULT_SCREEN_DPI);
}

void Window::ApplyDpiScaling(HWND hWnd)
{
    static const GetDpiForWindowFn pGetDpiForWindow = ResolveGetDpiForWindow();
    m_Dpi = pGetDpiForWindow ? static_cast<int>(pGetDpiForWindow(hWnd)) : USER_DEFAULT_SCREEN_DPI;

    if (m_Dpi != USER_DEFAULT_SCREEN_DPI) {
        SetWindowPos(hWnd, nullptr, 0, 0, Scale(m_Width), Scale(m_Height), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void Window::OnDpiChanged(const WPARAM wParam, const LPARAM lParam)
{
    m_Dpi = HIWORD(wParam);
    const auto suggested = reinterpret_cast<const RECT*>(lParam);
    SetWindowPos(
        m_hWnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE
    );
}

bool Window::Create()
{
    m_hEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (m_hEvent == nullptr) {
        fprintf(stderr, "CreateEventW failed (%lu)\n", GetLastError());
        return false;
    }

    NONCLIENTMETRICSW metrics = {0};
    metrics.cbSize = sizeof(metrics);
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0)) {
        fprintf(stderr, "SystemParametersInfoW failed (%lu)", GetLastError());
        return false;
    }

    m_hFont = CreateFontIndirectW(&metrics.lfMessageFont);
    if (m_hFont == nullptr) {
        fprintf(stderr, "CreateFontIndirectW failed\n");
        return false;
    }

    m_hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_ICON1));
    if (m_hIcon == nullptr) {
        fprintf(stderr, "LoadIconW failed (%lu)\n", GetLastError());
        return false;
    }

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = m_hInstance;
    wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
    wc.lpszClassName = L"GWToolbox-Window-Class";

    if (!RegisterClassW(&wc)) {
        DWORD LastError = GetLastError();
        if (LastError != ERROR_CLASS_ALREADY_EXISTS) {
            fprintf(stderr, "RegisterClassW failed (%lu)\n", LastError);
            return false;
        }
    }

    m_hWnd = CreateWindowW(
        wc.lpszClassName,
        m_WindowName,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        m_X,
        m_Y,
        m_Width,
        m_Height,
        nullptr,
        nullptr,
        m_hInstance,
        this);

    SendMessage(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)m_hIcon);
    SendMessage(m_hWnd, WM_SETICON, ICON_BIG, (LPARAM)m_hIcon);

    ShowWindow(m_hWnd, SW_SHOW);

    if (m_hWnd == nullptr) {
        fprintf(stderr, "CreateWindowW failed (%lu)\n", GetLastError());
        return false;
    }

    return true;
}

bool Window::WaitMessages() const
{
    MSG msg;
    for (;;) {
        const DWORD dwRet = MsgWaitForMultipleObjects(
            1,
            &m_hEvent,
            FALSE,
            INFINITE,
            QS_ALLINPUT);

        if (dwRet == WAIT_OBJECT_0) {
            break;
        }

        if (dwRet == WAIT_FAILED) {
            fprintf(stderr, "MsgWaitForMultipleObjects failed (%lu)\n", GetLastError());
            return false;
        }

        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return true;
}

bool Window::PollMessages(const uint32_t TimeoutMs) const
{
    MSG msg;

    const DWORD dwRet = MsgWaitForMultipleObjects(
        1,
        &m_hEvent,
        FALSE,
        TimeoutMs,
        QS_ALLINPUT);

    if (dwRet == WAIT_OBJECT_0 || dwRet == WAIT_TIMEOUT) {
        return true;
    }

    if (dwRet == WAIT_FAILED) {
        fprintf(stderr, "MsgWaitForMultipleObjects failed (%lu)\n", GetLastError());
        return false;
    }

    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return true;
}

bool Window::ProcessMessages()
{
    MSG msg;

    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return true;
}

void Window::SignalStop() const
{
    SetEvent(m_hEvent);
}

bool Window::ShouldClose() const
{
    const DWORD dwReason = WaitForSingleObject(m_hEvent, 0);

    if (dwReason == WAIT_TIMEOUT) {
        return false;
    }

    if (dwReason != WAIT_OBJECT_0) {
        fprintf(stderr, "WaitForSingleObject failed (%lu)\n", GetLastError());
    }

    return true;
}

void Window::SetWindowName(const LPCWSTR lpName)
{
    m_WindowName = lpName;
}

void Window::SetWindowPosition(const int X, const int Y)
{
    m_X = X;
    m_Y = Y;
}

void Window::SetWindowDimension(const int Width, const int Height)
{
    m_Width = Width;
    m_Height = Height;
}
