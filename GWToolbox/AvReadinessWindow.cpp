#include "stdafx.h"

#include <thread>

#include <Path.h>
#include "AvReadinessWindow.h"

namespace {
    constexpr int kWidth = 600, kHeight = 380;
    constexpr int kMargin = 16, kRowHeight = 28, kRowsTop = 92;
    constexpr int kIdRecheck = 1001, kIdContinue = 1002, kIdRowButtonBase = 2000;
    constexpr UINT kTimerId = 1, kPollIntervalMs = 4000;
    constexpr UINT WM_APP_READINESS = WM_APP + 1; // worker -> UI: heap AvReadiness* in lParam, ownership transferred
}

bool AvReadinessWindow::Create()
{
    SetWindowName(L"GWToolbox - Antivirus setup");
    SetWindowDimension(kWidth, kHeight);
    return Window::Create();
}

void AvReadinessWindow::Run()
{
    AvReadinessWindow window;
    if (!PathGetProgramDirectory(window.m_program_dir)) return;
    PathGetDocumentsPath(window.m_data_dir, L"GWToolboxpp");
    if (!window.Create()) return;
    while (!window.ShouldClose())
        window.PollMessages(50);
}

void AvReadinessWindow::OnCreate(const HWND hWnd)
{
    m_hWnd = hWnd; // WM_CREATE fires before Window::Create assigns m_hWnd; the background check needs it now
    const auto label = [&](const wchar_t* text, const int x, const int y, const int w, const int h, const DWORD style, const int id) {
        const HWND ctl = CreateWindowW(WC_STATICW, text, WS_VISIBLE | WS_CHILD | style, x, y, w, h, hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), m_hInstance, nullptr);
        SendMessageW(ctl, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        return ctl;
    };
    const auto button = [&](const wchar_t* text, const int x, const int y, const int w, const int h, const int id) {
        const HWND ctl = CreateWindowW(WC_BUTTONW, text, WS_VISIBLE | WS_CHILD | WS_TABSTOP, x, y, w, h, hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), m_hInstance, nullptr);
        SendMessageW(ctl, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        return ctl;
    };

    m_hHeader = label(L"Some antivirus software quarantines GWToolbox's files. The checks below confirm your antivirus is set up so Toolbox can run and save its settings. GWToolbox never changes these settings for you - the buttons just open the right Windows Security page.", kMargin, kMargin, kWidth - 2 * kMargin - 16, 64, 0, 0);
    m_hStatus = label(L"Checking...", kMargin, kHeight - 88, kWidth - 2 * kMargin - 16, 24, 0, 0);
    m_hRecheck = button(L"Re-check", kMargin, kHeight - 56, 120, 28, kIdRecheck);
    m_hContinue = button(L"Continue", kWidth - kMargin - 16 - 140, kHeight - 56, 140, 28, kIdContinue);
    StartCheck();
    SetTimer(hWnd, kTimerId, kPollIntervalMs, nullptr);
}

void AvReadinessWindow::StartCheck()
{
    if (m_checking.exchange(true)) return; // a query is already running
    EnableWindow(m_hRecheck, FALSE);
    const auto program_dir = m_program_dir;
    const auto data_dir = m_data_dir;
    const HWND hwnd = m_hWnd;
    std::thread([program_dir, data_dir, hwnd] {
        auto* result = new AvReadiness(GetAvReadiness(program_dir, data_dir));
        if (!PostMessageW(hwnd, WM_APP_READINESS, 0, reinterpret_cast<LPARAM>(result))) delete result; // window gone
    }).detach();
}

void AvReadinessWindow::ApplyReadiness()
{
    for (const HWND h : m_row_labels) DestroyWindow(h);
    for (const HWND h : m_row_buttons)
        if (h) DestroyWindow(h);
    m_row_labels.clear();
    m_row_buttons.clear();
    m_row_satisfied.clear();

    bool all_ok = true;
    for (size_t i = 0; i < m_readiness.checks.size(); i++) {
        const AvCheck& c = m_readiness.checks[i];
        all_ok = all_ok && c.satisfied;
        const int y = kRowsTop + static_cast<int>(i) * kRowHeight;
        const std::wstring text = (c.satisfied ? L"✓  " : L"✗  ") + c.label;
        const HWND lbl = CreateWindowW(WC_STATICW, text.c_str(), WS_VISIBLE | WS_CHILD | SS_LEFT, kMargin, y, kWidth - 2 * kMargin - 150, kRowHeight - 4, m_hWnd, nullptr, m_hInstance, nullptr);
        SendMessageW(lbl, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        HWND btn = nullptr;
        if (!c.satisfied && !c.settings_uri.empty()) {
            btn = CreateWindowW(WC_BUTTONW, L"Open Windows Security", WS_VISIBLE | WS_CHILD | WS_TABSTOP, kWidth - kMargin - 16 - 170, y - 2, 170, kRowHeight - 2, m_hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRowButtonBase + i)), m_hInstance, nullptr);
            SendMessageW(btn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        }
        m_row_labels.push_back(lbl);
        m_row_buttons.push_back(btn);
        m_row_satisfied.push_back(c.satisfied);
    }

    std::wstring status;
    if (!m_readiness.read_ok)
        status = L"Couldn't read your antivirus settings - if Toolbox runs fine, you can continue.";
    else if (!m_readiness.third_party_av.empty())
        status = std::format(L"Detected {}. Add the folders above to its exclusions, then continue.", m_readiness.third_party_av);
    else if (all_ok)
        status = L"All set - your antivirus won't get in Toolbox's way.";
    else
        status = L"Add the items marked ✗, then they'll tick automatically.";
    SetWindowTextW(m_hStatus, status.c_str());
    SetWindowTextW(m_hContinue, all_ok ? L"Continue" : L"Continue anyway");
    if (all_ok) KillTimer(m_hWnd, kTimerId); // nothing left to watch for
    InvalidateRect(m_hWnd, nullptr, TRUE);
}

LRESULT AvReadinessWindow::WndProc(const HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    switch (uMsg) {
        case WM_CREATE:
            OnCreate(hWnd);
            break;

        case WM_APP_READINESS: {
            const auto* result = reinterpret_cast<AvReadiness*>(lParam);
            m_readiness = *result;
            delete result;
            m_checking = false;
            EnableWindow(m_hRecheck, TRUE);
            ApplyReadiness();
            break;
        }

        case WM_TIMER:
            StartCheck();
            break;

        case WM_ACTIVATE:
            if (LOWORD(wParam) != WA_INACTIVE) StartCheck(); // returning from Windows Security: re-check at once
            break;

        case WM_CTLCOLORSTATIC: {
            const auto target = reinterpret_cast<HWND>(lParam);
            for (size_t i = 0; i < m_row_labels.size(); i++) {
                if (m_row_labels[i] != target) continue;
                const auto hdc = reinterpret_cast<HDC>(wParam);
                SetTextColor(hdc, m_row_satisfied[i] ? RGB(0, 128, 0) : RGB(192, 0, 0));
                SetBkMode(hdc, TRANSPARENT);
                return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_3DFACE));
            }
            break;
        }

        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            if (id == kIdContinue) {
                DestroyWindow(hWnd);
            }
            else if (id == kIdRecheck) {
                StartCheck();
            }
            else if (id >= kIdRowButtonBase && id - kIdRowButtonBase < static_cast<int>(m_readiness.checks.size())) {
                const std::wstring& uri = m_readiness.checks[id - kIdRowButtonBase].settings_uri;
                // Fall back to the Windows Security home if a build doesn't accept the specific page moniker.
                if (reinterpret_cast<INT_PTR>(ShellExecuteW(hWnd, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
                    ShellExecuteW(hWnd, L"open", L"windowsdefender://", nullptr, nullptr, SW_SHOWNORMAL);
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(hWnd);
            break;

        case WM_DESTROY:
            KillTimer(hWnd, kTimerId);
            SignalStop();
            break;

        default:
            break;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
