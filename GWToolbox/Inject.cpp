#include "stdafx.h"

#include "Inject.h"
#include "Process.h"
#include "Settings.h"

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
    inject.Create();

    for (size_t i = 0; i < charnames.size(); i++)
    {
        const wchar_t *name = charnames[i].c_str();
        SendMessageW(inject.m_hCharacters, CB_ADDSTRING, 0, (LPARAM)name);
    }
    SendMessageW(inject.m_hCharacters, CB_SETCURSEL, 0, 0);

    inject.WaitMessages();

    size_t index;
    if (!inject.GetSelected(&index)) {
        fprintf(stderr, "No index selected\n");
        return InjectReply_Cancel;
    }

    *target_process = std::move(valid_processes[index]);
    return InjectReply_Inject;
}

InjectWindow::InjectWindow()
    : m_hCharacters(nullptr)
    , m_hLaunchButton(nullptr)
    , m_hRestartAsAdmin(nullptr)
    , m_hSettings(nullptr)
    , m_Selected(-1)
{
}

InjectWindow::~InjectWindow()
{
}

bool InjectWindow::Create()
{
    SetWindowName(L"GWToolbox - Launch");
    SetWindowDimension(305, 135);
    return Window::Create();
}

bool InjectWindow::GetSelected(size_t *index)
{
    if (m_Selected >= 0) {
        *index = static_cast<size_t>(m_Selected);
        return true;
    } else {
        return false;
    }
}

LRESULT InjectWindow::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        OnCreate(hWnd, uMsg, wParam, lParam);
        break;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        SignalStop();
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

void InjectWindow::OnCreate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

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
        m_hInstance,
        nullptr);
    SendMessageW(hGroupBox, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));

    m_hCharacters = CreateWindowW(
        WC_COMBOBOXW,
        L"",
        WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST,
        20,  // x
        25,  // y
        155,// width
        25,  // height
        hWnd,
        nullptr,
        m_hInstance,
        nullptr);
    SendMessageW(m_hCharacters, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));

    m_hLaunchButton = CreateWindowW(
        WC_BUTTONW,
        L"Launch",
        WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON,
        180, // x
        24,  // y
        90,  // width
        25,  // height
        hWnd,
        nullptr,
        m_hInstance,
        nullptr);
    SendMessageW(m_hLaunchButton, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));

    if (!IsRunningAsAdmin())
    {
        m_hRestartAsAdmin = CreateWindowW(
            WC_BUTTONW,
            L"Can't find your character?",
            WS_VISIBLE | WS_CHILD | WS_TABSTOP,
            10,
            65,
            165,
            25,
            hWnd,
            nullptr,
            m_hInstance,
            nullptr);
        SendMessageW(m_hRestartAsAdmin, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));
        Button_SetElevationRequiredState(m_hRestartAsAdmin, TRUE);
    }

    m_hSettings = CreateWindowW(
        WC_BUTTONW,
        L"Settings...",
        WS_VISIBLE | WS_CHILD | WS_TABSTOP,
        200,
        65,
        80,
        25,
        hWnd,
        nullptr,
        m_hInstance,
        nullptr);
    SendMessageW(m_hSettings, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));
}

void InjectWindow::OnCommand(HWND hWnd, LONG ControlId, LONG NotificationCode)
{
    UNREFERENCED_PARAMETER(NotificationCode);

    if ((hWnd == m_hLaunchButton) && (ControlId == STN_CLICKED)) {
        m_Selected = SendMessageW(m_hCharacters, CB_GETCURSEL, 0, 0);
        DestroyWindow(m_hWnd);
    } else if ((hWnd == m_hRestartAsAdmin) && (ControlId == STN_CLICKED)) {
        RestartAsAdminWithSameArgs();
    } else if ((hWnd == m_hSettings) && (ControlId == STN_CLICKED)) {
        SettingsWindow window;
        window.Create();
        // @Remark:
        // This is kind of dumb, we block here, but it works, because the
        // message dispatcher doesn't care about the object.
        // We should improve that...
        window.WaitMessages();
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
