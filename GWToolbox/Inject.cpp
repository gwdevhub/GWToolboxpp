#include <stdio.h>

#include "Inject.h"
#include "Process.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

bool InjectWindow::AskInjectProcess(Process *target_process)
{
    std::vector<Process> processes;
    if (!GetProcesses(processes, L"Gw.exe")) {
    }

    if (!GetProcessesFromWindowClass(processes, L"ArenaNet_Dx_Window_Class")) {
    }

    if (processes.empty()) {
        // MessageBoxW(0, L"Error: Guild Wars not running",  L"GWToolbox++", MB_ICONERROR);
        return false;
    }

    uintptr_t charname_rva;
    ProcessScanner scanner(&processes[0]);
    if (!scanner.FindPatternRva("\x8B\xF8\x6A\x03\x68\x0F\x00\x00\xC0\x8B\xCF\xE8", "xxxxxxxxxxxx", -0x42, &charname_rva)) {
        MessageBoxW(0, L"Error: Couldn't find charname rva", L"GWToolbox++", MB_ICONERROR);
        return false;
    }

    std::vector<std::wstring> charnames;
    charnames.reserve(processes.size());
    for (Process& process : processes) {
        ProcessModule module;
        if (!process.GetModule(&module)) {
            // Add logging
            charnames.emplace_back(L"");
            continue;
        }

        uint32_t charname_ptr;
        if (!process.Read(module.base + charname_rva, &charname_ptr, 4)) {
            // Add logging
            charnames.emplace_back(L"");
            continue;
        }

        wchar_t charname[32] = {};
        if (!process.Read(charname_ptr, &charname, 32)) {
            // Add logging
            charnames.emplace_back(L"");
            continue;
        }

        size_t charname_len = wcsnlen(charname, _countof(charname));
        charnames.emplace_back(charname, charname + charname_len);
    }

    InjectWindow inject;
    inject.Create(L"GWToolbox", charnames);

    for (size_t i = 0; i < charnames.size(); i++)
    {
        const wchar_t *name = charnames[i].c_str();
        // Add string to combobox.
        SendMessageW(inject.m_characters_combo, CB_ADDSTRING, (WPARAM)0, (LPARAM)name);
    }

    inject.WaitMessages();
    *target_process = std::move(processes[inject.m_selected]);
    return true;
}

void InjectWindow::OnWindowCreate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LPCREATESTRUCTW param = reinterpret_cast<LPCREATESTRUCTW>(lParam);
    InjectWindow *inject = static_cast<InjectWindow *>(param->lpCreateParams);

    SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG>(inject));

    inject->m_inject_button = CreateWindowW(
        L"BUTTON",
        L"Inject",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        7,
        67,
        140,
        24,
        hWnd,
        NULL,
        inject->m_instance,
        NULL);

    inject->m_characters_combo = CreateWindowW(
        L"COMBOBOX",
        L"",
        CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP | WS_VISIBLE | WS_CHILD,
        7,
        11,
        140,
        300,
        hWnd,
        NULL,
        inject->m_instance,
        NULL);
}

LRESULT CALLBACK InjectWindow::MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LONG_PTR ud = GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    InjectWindow *inject = reinterpret_cast<InjectWindow *>(ud);

    switch (uMsg)
    {
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        SetEvent(inject->m_event);
        break;

    case WM_CREATE:
        InjectWindow::OnWindowCreate(hWnd, uMsg, wParam, lParam);
        break;

    case WM_COMMAND:
        inject->OnEvent(reinterpret_cast<HWND>(lParam), LOWORD(wParam), HIWORD(wParam));
        break;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

InjectWindow::InjectWindow()
    : m_window(nullptr)
    , m_characters_combo(nullptr)
    , m_inject_button(nullptr)
    , m_event(nullptr)
    , m_instance(nullptr)
    , m_selected(0)
{
}

InjectWindow::~InjectWindow()
{
    CloseHandle(m_event);
}

bool InjectWindow::Create(const wchar_t *title, std::vector<std::wstring>& names)
{
    m_instance = GetModuleHandleW(NULL);

    m_event = CreateEventW(0, FALSE, FALSE, NULL);
    if (m_event == NULL)
    {
        fprintf(stderr, "CreateEventW failed (%lu)\n", GetLastError());
        return false;
    }

    WNDCLASSW wc = {0};
    // wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = m_instance;
    wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
    wc.lpszClassName = L"GWToolbox-Inject-Window-Class";

    if (!RegisterClassW(&wc))
    {
        fprintf(stderr, "RegisterClassW failed (%lu)\n", GetLastError());
        return false;
    }

    m_window = CreateWindowW(
        wc.lpszClassName,
        title,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, // x
        CW_USEDEFAULT, // y
        170, // width
        140, // height
        NULL,
        NULL,
        m_instance,
        this);

    if (m_window == NULL)
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
            &m_event,
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

        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    DestroyWindow(m_window);
    return true;
}

void InjectWindow::OnEvent(HWND hwnd, LONG control_id, LONG notification_code)
{
    // printf("hwnd:%p, control_id:%ld, notification_code:%lu\n", hwnd, control_id, notification_code);
    if ((hwnd == m_inject_button) && (control_id == 0)) {
        m_selected = SendMessageW(m_characters_combo, CB_GETCURSEL, 0, 0);
        printf("Pressed: %d\n", m_selected);
        SetEvent(m_event);
    }
}

static LPVOID GetLoadLibrary()
{
    HMODULE Kernel32 = GetModuleHandleW(L"Kernel32.dll");
    if (Kernel32 == NULL)
    {
        fprintf(stderr, "GetModuleHandleW failed (%lu)\n", GetLastError());
        return NULL;
    }

    LPVOID pLoadLibraryW = GetProcAddress(Kernel32, "LoadLibraryW");
    if (pLoadLibraryW == NULL)
    {
        fprintf(stderr, "GetProcAddress failed (%lu)\n", GetLastError());
        return NULL;
    }

    return pLoadLibraryW;
}

bool InjectRemoteThread(Process& process, LPCWSTR ImagePath, LPDWORD lpExitCode)
{
    *lpExitCode = 0;

    HANDLE ProcessHandle = process.GetHandle();
    if (ProcessHandle == NULL)
    {
        fprintf(stderr, "Can't inject a dll in a process which is not open\n");
        return FALSE;
    }

    LPVOID pLoadLibraryW = GetLoadLibrary();
    if (pLoadLibraryW == NULL)
        return FALSE;

    size_t ImagePathLength = wcslen(ImagePath);
    size_t ImagePathSize = (ImagePathLength * 2) + 2;

    LPVOID ImagePathAddress = VirtualAllocEx(
        ProcessHandle,
        NULL,
        ImagePathSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);

    if (ImagePathAddress == NULL)
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
        NULL,
        0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(pLoadLibraryW),
        ImagePathAddress,
        0,
        NULL,
        &ThreadId);

    if (hThread == NULL)
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
