#include "stdafx.h"

#include "Inject.h"
#include "Options.h"
#include "Process.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// @Cleanup: @Remark:
// According to Microsoft documentation for BCM_SETSHIELD:
// > An application must be manifested to use comctl32.dll
// > version 6 to gain this functionality.
// If we don't do that, the shield icon doesn't show up, but is there
// a nice way to add that? (i.e. not through a pragma)
#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

InjectReply InjectWindow::AskInjectProcess(Process *target_process)
{
    std::vector<Process> processes;
    GetProcesses(processes, L"Gw.exe");
    GetProcessesFromWindowClass(processes, L"ArenaNet_Dx_Window_Class");

    if (processes.empty()) {
        fprintf(stderr, "Didn't find any potential process to inject GWToolbox\n");
        return InjectReply_NoProcess;
    }

    uintptr_t charname_rva;
    ProcessScanner scanner(&processes[0]);
    if (!scanner.FindPatternRva("\x8B\xF8\x6A\x03\x68\x0F\x00\x00\xC0\x8B\xCF\xE8", "xxxxxxxxxxxx", -0x42, &charname_rva)) {
        return InjectReply_PatternError;
    }

    std::vector<std::wstring> charnames;
    charnames.reserve(processes.size());
    std::vector<Process> valid_processes;
    valid_processes.reserve(processes.size());

    for (Process& process : processes) {
        ProcessModule module;

        if (!process.GetModule(&module)) {
            fprintf(stderr, "Couldn't get module for process %u\n", process.GetProcessId());
            continue;
        }

        uint32_t charname_ptr;
        if (!process.Read(module.base + charname_rva, &charname_ptr, 4)) {
            fprintf(stderr, "Can't read the address 0x%08X in process %u\n",
                module.base + charname_rva, process.GetProcessId());
            continue;
        }

        wchar_t charname[32];
        if (!process.Read(charname_ptr, charname, 40)) {
            fprintf(stderr, "Can't read the character name at address 0x%08X in process %u\n",
                charname_ptr, process.GetProcessId());
            continue;
        }
        charname[20] = 0;

        if (charname[0] == 0) {
            fprintf(stderr, "Character name in process %u is empty\n", process.GetProcessId());
            continue;
        }

        size_t charname_len = wcsnlen(charname, _countof(charname));
        charnames.emplace_back(charname, charname + charname_len);
        valid_processes.emplace_back(std::move(process));
    }

    processes.clear();

    InjectWindow inject;
    inject.Create(charnames);

    for (size_t i = 0; i < charnames.size(); i++)
    {
        const wchar_t *name = charnames[i].c_str();
        SendMessageW(inject.m_hCharacters, CB_ADDSTRING, 0, (LPARAM)name);
    }
    SendMessageW(inject.m_hCharacters, CB_SETCURSEL, 0, 0);

    inject.WaitMessages();

    int index;
    if (!inject.GetSelected(&index)) {
        fprintf(stderr, "No index selected\n");
        return InjectReply_Cancel;
    }

    *target_process = std::move(valid_processes[index]);
    return InjectReply_Inject;
}

void InjectWindow::OnWindowCreate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LPCREATESTRUCTW param = reinterpret_cast<LPCREATESTRUCTW>(lParam);
    InjectWindow *inject = static_cast<InjectWindow *>(param->lpCreateParams);

    SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG>(inject));

    HICON hIcon = LoadIconW(inject->m_hInstance, MAKEINTRESOURCEW(IDI_ICON1));
    SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

    HWND hGroupBox = CreateWindowW(
        WC_BUTTONW,
        L"Select Character",
        WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
        10,
        5,
        270,
        55,
        hWnd,
        nullptr,
        inject->m_hInstance,
        nullptr);
    SendMessageW(hGroupBox, WM_SETFONT, (WPARAM)inject->m_hFont, MAKELPARAM(TRUE, 0));

    inject->m_hCharacters = CreateWindowW(
        WC_COMBOBOXW,
        L"",
        WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST,
        20,  // x
        25,  // y
        155,// width
        25,  // height
        hWnd,
        nullptr,
        inject->m_hInstance,
        nullptr);
    SendMessageW(inject->m_hCharacters, WM_SETFONT, (WPARAM)inject->m_hFont, MAKELPARAM(TRUE, 0));

    inject->m_hLaunchButton = CreateWindowW(
        WC_BUTTONW,
        L"Launch",
        WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON,
        180, // x
        24,  // y
        90,  // width
        25,  // height
        hWnd,
        nullptr,
        inject->m_hInstance,
        nullptr);
    SendMessageW(inject->m_hLaunchButton, WM_SETFONT, (WPARAM)inject->m_hFont, MAKELPARAM(TRUE, 0));

    if (!IsRunningAsAdmin())
    {
        inject->m_hRestartAsAdmin = CreateWindowW(
            WC_BUTTONW,
            L"Can't find your character?",
            WS_VISIBLE | WS_CHILD | WS_TABSTOP,
            10,
            65,
            165,
            25,
            hWnd,
            nullptr,
            inject->m_hInstance,
            nullptr);
        SendMessageW(inject->m_hRestartAsAdmin, WM_SETFONT, (WPARAM)inject->m_hFont, MAKELPARAM(TRUE, 0));
        Button_SetElevationRequiredState(inject->m_hRestartAsAdmin, TRUE);
    }

    inject->m_hSettings = CreateWindowW(
        WC_BUTTONW,
        L"Settings...",
        WS_VISIBLE | WS_CHILD | WS_TABSTOP,
        200,
        65,
        80,
        25,
        hWnd,
        nullptr,
        inject->m_hInstance,
        nullptr);
    SendMessageW(inject->m_hSettings, WM_SETFONT, (WPARAM)inject->m_hFont, MAKELPARAM(TRUE, 0));

#if 0
    L"Unable to retrieve all necessary information for some process"
    L"Some processes couldn't be listed" -> restart as admin

    HWND hCheckUpdate = CreateWindowW(
        WC_BUTTONW,
        L"Check for update",
        WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON,
        10,
        65,
        100,
        25,
        hWnd,
        nullptr,
        inject->m_hInstance,
        nullptr);
    SendMessageW(hCheckUpdate, WM_SETFONT, (WPARAM)inject->m_hFont, MAKELPARAM(TRUE, 0));

    HWND hSkipUpdate = CreateWindowW(
        WC_BUTTONW,
        L"Check for update",
        WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_CHECKBOX,
        15,
        65,
        150,
        15,
        hWnd,
        nullptr,
        inject->m_hInstance,
        nullptr);
    SendMessageW(hSkipUpdate, WM_SETFONT, (WPARAM)inject->m_hFont, MAKELPARAM(TRUE, 0));

    HWND hStartAsAdmin = CreateWindowW(
        WC_BUTTONW,
        L"Start as admin",
        WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_CHECKBOX,
        15,
        85,
        150,
        15,
        hWnd,
        nullptr,
        inject->m_hInstance,
        nullptr);
    SendMessageW(hStartAsAdmin, WM_SETFONT, (WPARAM)inject->m_hFont, MAKELPARAM(TRUE, 0));
#endif
}

LRESULT CALLBACK InjectWindow::MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LONG_PTR ud = GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    InjectWindow *window = reinterpret_cast<InjectWindow *>(ud);

    switch (uMsg)
    {
    case WM_CREATE:
        InjectWindow::OnWindowCreate(hWnd, uMsg, wParam, lParam);
        break;

    default:
        return window->WndProc(hWnd, uMsg, wParam, lParam);
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

InjectWindow::InjectWindow()
    : m_hWnd(nullptr)
    , m_hCharacters(nullptr)
    , m_hLaunchButton(nullptr)
    , m_hRestartAsAdmin(nullptr)
    , m_hSettings(nullptr)
    , m_hFont(nullptr)
    , m_hEvent(nullptr)
    , m_hInstance(nullptr)
    , m_Selected(-1)
{
}

InjectWindow::~InjectWindow()
{
    if (m_hEvent)
        CloseHandle(m_hEvent);
    if (m_hFont)
        DeleteObject(m_hFont);
}

bool InjectWindow::Create(std::vector<std::wstring>& names)
{
    // @Enhancement:
    // Should we reset the class here? (e.g. m_Selected)

    m_hInstance = GetModuleHandleW(nullptr);

    NONCLIENTMETRICSW metrics = {0};
    metrics.cbSize = sizeof(metrics);
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0))
    {
        fprintf(stderr, "SystemParametersInfoW failed (%lu)", GetLastError());
        return false;
    }

    m_hFont = CreateFontIndirectW(&metrics.lfMessageFont);
    if (m_hFont == nullptr)
    {
        fprintf(stderr, "CreateFontIndirectW failed\n");
        return false;
    }

    m_hEvent = CreateEventW(0, FALSE, FALSE, nullptr);
    if (m_hEvent == nullptr)
    {
        fprintf(stderr, "CreateEventW failed (%lu)\n", GetLastError());
        return false;
    }

    WNDCLASSW wc = {0};
    // wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = m_hInstance;
    wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
    wc.lpszClassName = L"GWToolbox-Inject-Window-Class";

    if (!RegisterClassW(&wc))
    {
        fprintf(stderr, "RegisterClassW failed (%lu)\n", GetLastError());
        return false;
    }

    m_hWnd = CreateWindowW(
        wc.lpszClassName,
        L"GWToolbox",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, // x
        CW_USEDEFAULT, // y
        305, // width
        135, // height
        nullptr,
        nullptr,
        m_hInstance,
        this);

    ShowWindow(m_hWnd, SW_SHOW);

    if (m_hWnd == nullptr)
    {
        fprintf(stderr, "CreateWindowW failed (%lu)\n", GetLastError());
        return false;
    }

    return true;
}

bool InjectWindow::WaitMessages()
{
    MSG msg;
    for (;;)
    {
        DWORD dwRet = MsgWaitForMultipleObjects(
            1,
            &m_hEvent,
            FALSE,
            INFINITE,
            QS_ALLINPUT);

        if (dwRet == WAIT_OBJECT_0)
            break;

        if (dwRet == WAIT_FAILED)
        {
            fprintf(stderr, "MsgWaitForMultipleObjects failed (%lu)\n", GetLastError());
            return false;
        }

        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return true;
}

bool InjectWindow::GetSelected(int *index)
{
    if (m_Selected >= 0) {
        *index = m_Selected;
        return true;
    } else {
        return false;
    }
}

LRESULT InjectWindow::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        SetEvent(m_hEvent);
        break;

    case WM_COMMAND:
        OnCommand(reinterpret_cast<HWND>(lParam), LOWORD(wParam), HIWORD(wParam));
        break;

    case WM_KEYUP:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hWnd);
        } else if (wParam == VK_RETURN) {
            m_Selected = SendMessageW(m_hCharacters, CB_GETCURSEL, 0, 0);
            DestroyWindow(hWnd);
        }
        break;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

void InjectWindow::OnCommand(HWND hwnd, LONG control_id, LONG notification_code)
{
    if ((hwnd == m_hLaunchButton) && (control_id == STN_CLICKED)) {
        m_Selected = SendMessageW(m_hCharacters, CB_GETCURSEL, 0, 0);
        DestroyWindow(m_hWnd);
    } else if ((hwnd == m_hRestartAsAdmin) && (control_id == STN_CLICKED)) {
        RestartAsAdminWithSameArgs();
    }
}

static LPVOID GetLoadLibrary()
{
    HMODULE Kernel32 = GetModuleHandleW(L"Kernel32.dll");
    if (Kernel32 == nullptr)
    {
        fprintf(stderr, "GetModuleHandleW failed (%lu)\n", GetLastError());
        return nullptr;
    }

    LPVOID pLoadLibraryW = GetProcAddress(Kernel32, "LoadLibraryW");
    if (pLoadLibraryW == nullptr)
    {
        fprintf(stderr, "GetProcAddress failed (%lu)\n", GetLastError());
        return nullptr;
    }

    return pLoadLibraryW;
}

bool InjectRemoteThread(Process *process, LPCWSTR ImagePath, LPDWORD lpExitCode)
{
    *lpExitCode = 0;

    HANDLE ProcessHandle = process->GetHandle();
    if (ProcessHandle == nullptr)
    {
        fprintf(stderr, "Can't inject a dll in a process which is not open\n");
        return FALSE;
    }

    LPVOID pLoadLibraryW = GetLoadLibrary();
    if (pLoadLibraryW == nullptr)
        return FALSE;

    size_t ImagePathLength = wcslen(ImagePath);
    size_t ImagePathSize = (ImagePathLength * 2) + 2;

    LPVOID ImagePathAddress = VirtualAllocEx(
        ProcessHandle,
        nullptr,
        ImagePathSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);

    if (ImagePathAddress == nullptr)
    {
        fprintf(stderr, "VirtualAllocEx failed (%lu)\n", GetLastError());
        return FALSE;
    }

    SIZE_T BytesWritten;
    BOOL Success = WriteProcessMemory(
        ProcessHandle,
        ImagePathAddress,
        ImagePath,
        ImagePathSize,
        &BytesWritten);

    if (!Success || (ImagePathSize != BytesWritten))
    {
        fprintf(stderr, "WriteProcessMemory failed (%lu)\n", GetLastError());
        VirtualFreeEx(ProcessHandle, ImagePathAddress, 0, MEM_RELEASE);
        return FALSE;
    }

    DWORD ThreadId;
    HANDLE hThread = CreateRemoteThreadEx(
        ProcessHandle,
        nullptr,
        0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(pLoadLibraryW),
        ImagePathAddress,
        0,
        nullptr,
        &ThreadId);

    if (hThread == nullptr)
    {
        fprintf(stderr, "CreateRemoteThreadEx failed (%lu)\n", GetLastError());
        return FALSE;
    }

    DWORD Reason = WaitForSingleObject(hThread, INFINITE);
    if (Reason != WAIT_OBJECT_0)
    {
        fprintf(stderr, "WaitForSingleObject failed {reason: %lu, error: %lu}\n", Reason, GetLastError());
        CloseHandle(hThread);
        return FALSE;
    }

    VirtualFreeEx(ProcessHandle, ImagePathAddress, 0, MEM_RELEASE);

    DWORD ExitCode;
    Success = GetExitCodeThread(hThread, &ExitCode);
    CloseHandle(hThread);

    if (Success == FALSE)
    {
        fprintf(stderr, "GetExitCodeThread failed (%lu)\n", GetLastError());
        return FALSE;
    }

    *lpExitCode = ExitCode;
    return TRUE;
}
