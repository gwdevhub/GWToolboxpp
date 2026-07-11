#include "stdafx.h"

#include <Defender.h>
#include <Str.h>

#include "Inject.h"

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

static bool FindTopMostProcess(const std::vector<InjectProcess>& processes, size_t* top_most_index)
{
    if (processes.size() >= 250) {
        fprintf(
            stderr, "Process::FindTopMostProcess is O(n^2) where n is the number of processes."
                    "Consider rewriting the function to have a better scaling for large number of processes.\n"
        );
    }

    HWND hWndIt = GetTopWindow(nullptr);
    if (hWndIt == nullptr) {
        fprintf(stderr, "GetTopWindow failed (%lu)\n", GetLastError());
        return false;
    }

    while (hWndIt != nullptr) {
        DWORD WindowPid;
        if (GetWindowThreadProcessId(hWndIt, &WindowPid) == 0) {
            // @Cleanup:
            // Not clear whether this is the return value hold an error, so we just log.
            fprintf(stderr, "GetWindowThreadProcessId returned 0\n");
            continue;
        }

        for (size_t i = 0; i < processes.size(); ++i) {
            if (processes[i].m_Process.GetProcessId() == WindowPid) {
                *top_most_index = i;
                return true;
            }
        }

        hWndIt = GetWindow(hWndIt, GW_HWNDNEXT);
    }

    return true;
}


std::vector<Process> GetGuildWars2Processes()
{
    std::vector<Process> processes;
    GetProcessesByName(processes, L"Gw2-64.exe");
    GetProcessesByWindowClass(processes, L"ArenaNet_Gr_Window_Class");
    return processes;
}
std::vector<Process> GetGuildWarsProcesses()
{
    std::vector<Process> processes;
    GetProcessesByName(processes, L"Gw.exe");
    GetProcessesByWindowClass(processes, L"ArenaNet_Dialog_Class");
    return processes;
}

std::vector<std::filesystem::path> GetGuildWarsExecutablePaths()
{
    std::vector<std::filesystem::path> paths;
    for (auto& process : GetGuildWarsProcesses()) {
        std::wstring path;
        if (!process.GetPath(path))
            continue;
        std::filesystem::path exe(std::move(path));
        if (std::ranges::find(paths, exe) == paths.end())
            paths.push_back(std::move(exe));
    }
    return paths;
}

// Scans for injectable Guild Wars processes; callable repeatedly (initial load, and every Retry click).
static InjectScanResult RunScan()
{
    InjectScanResult result;

    std::vector<Process> processes = GetGuildWarsProcesses();

    if (processes.empty()) {
        fprintf(stderr, "Didn't find any potential process to inject GWToolbox\n");
        const bool gw2_running = !GetGuildWars2Processes().empty();
        result.m_ErrorMessage = gw2_running
                                     ? L"GWToolbox is for Guild Wars, not Guild Wars 2.\nStart Guild Wars, then click Retry."
                                     : L"Guild Wars isn't running.\nStart the game, then click Retry.";
        return result;
    }

    uintptr_t charname_rva = 0;
    uintptr_t email_rva = 0;
    bool read_blocked = false;
    DWORD read_error = 0;

    for (int i = 0; i < processes.size(); i++) {
        const ProcessScanner scanner(&processes[i]);
        if (!scanner.IsValid()) {
            // Couldn't read the GW image to scan it - almost always AV/anti-tamper stripping our access.
            read_blocked = true;
            read_error = scanner.GetError();
            continue;
        }
        if (!scanner.FindPatternRva("\x6a\x14\x83\xc0\x18\x50\x68", "xxxxxxx", 7, &charname_rva)) {
            continue;
        }

        if (!scanner.FindPatternRva("\x68\x80\x00\x00\x00\x51\x68", "xxxxxxx", 7, &email_rva)) {
            continue;
        }

        break;
    }

    if (!charname_rva || !email_rva) {
        if (read_blocked) {
            fprintf(stderr, "Couldn't read Guild Wars memory to scan for RVAs (error %lu) - likely anti-virus interference\n", read_error);
            std::wstring detail;
            if (FindRecentDefenderBlock(L"Gw.exe", 5, detail)) {
                fprintf(stderr, "Windows Defender reported: %ls\n", detail.c_str());
            }
            result.m_ErrorMessage = L"Couldn't read Guild Wars' memory to find your character.\nThis is usually antivirus or Controlled Folder Access blocking GWToolbox.";
            result.m_TroubleshootingUrl = Troubleshooting::CantReadMemory;
            return result;
        }
        fprintf(stderr, "Couldn't find charname/email RVAs in any potential process\n");
        result.m_ErrorMessage = L"Couldn't locate your character name in memory.\nUpdate GWToolbox or contact the developers.";
        return result;
    }

    std::vector<InjectProcess> inject_processes;

    for (Process& process : processes) {
        ProcessModule module;

        if (!process.GetModule(&module)) {
            fprintf(stderr, "Couldn't get module for process %lu\n", process.GetProcessId());
            continue;
        }

        bool injected;
        ProcessModule module2;
        if (process.GetModule(&module2, L"GWToolboxdll.dll")) {
            injected = true;
        }
        else {
            injected = false;
        }

        uint32_t charname_ptr;
        if (!process.Read(module.base + charname_rva, &charname_ptr, 4)) {
            fprintf(stderr, "Can't read the address 0x%08X in process %lu\n", module.base + charname_rva, process.GetProcessId());
            continue;
        }
        uint32_t email_ptr = 0;
        if (!process.Read(module.base + email_rva, &email_ptr, 4)) {
            fprintf(stderr, "Can't read the address 0x%08X in process %lu\n", module.base + email_rva, process.GetProcessId());
            continue;
        }
        wchar_t charname[128] = {0};
        if (!process.Read(charname_ptr, charname, 20 * sizeof(wchar_t))) {
            fprintf(stderr, "Can't read the character name at address 0x%08X in process %lu\n", charname_ptr, process.GetProcessId());
            continue;
        }
        if (!charname[0]) {
            char email[_countof(charname)] = {0};
            if (!process.Read(email_ptr, email, _countof(email) - 1)) {
                fprintf(stderr, "Can't read the email at address 0x%08X in process %lu\n", email_ptr, process.GetProcessId());
                continue;
            }
            for (int i = 0; i < _countof(email) && email[i]; i++) {
                charname[i] = email[i];
            }
        }
        if (!charname[0]) {
            fprintf(stderr, "Character name in process %lu is empty\n", process.GetProcessId());
            wcscpy_s(charname, sizeof(L"<No character selected>"), L"<No character selected>");
        }

        const size_t charname_len = wcsnlen(charname, _countof(charname));
        std::wstring charname2(charname, charname + charname_len);

        inject_processes.emplace_back(injected, std::move(process), std::wstring(charname2));
    }

    processes.clear();

    if (inject_processes.empty()) {
        fprintf(stderr, "No process with a readable character name\n");
        result.m_ErrorMessage = IsRunningAsAdmin()
                                     ? L"Couldn't find a valid character to inject into."
                                     : L"Couldn't find a valid character.\nGWToolbox may need administrator privileges.";
        return result;
    }

    // Sort by name
    std::ranges::sort(inject_processes, [](const InjectProcess& proc1, const InjectProcess& proc2) {
        return proc1.m_Charname < proc2.m_Charname;
    });

    result.m_Found = true;
    result.m_Processes = std::move(inject_processes);
    return result;
}

InjectReply InjectWindow::AskInjectProcess(Process* target_process)
{
    InjectScanResult scan = RunScan();

    if (settings.quiet) {
        if (scan.m_Found && scan.m_Processes.size() == 1) {
            *target_process = std::move(scan.m_Processes[0].m_Process);
            return InjectReply_Inject; // Inject if 1 process found
        }
        if (!scan.m_Found) {
            fprintf(stderr, "%ls\n", scan.m_ErrorMessage.c_str());
            return InjectReply_Cancel;
        }
        // Multiple candidates with no way to pick one non-interactively: fall through and show the window anyway.
    }

    InjectWindow inject;
    inject.SetInitialScanResult(std::move(scan));
    inject.Create();

    while (!inject.ShouldClose()) {
        inject.PollMessages(16);
        inject.Tick();
    }

    size_t index;
    if (!inject.GetSelected(&index)) {
        fprintf(stderr, "No index selected\n");
        return InjectReply_Cancel;
    }

    *target_process = inject.TakeSelectedProcess(index);
    return InjectReply_Inject;
}

InjectWindow::InjectWindow()
    : m_hCharacters(nullptr)
    , m_hLaunchButton(nullptr)
    , m_hErrorText(nullptr)
    , m_hRetryButton(nullptr)
    , m_hTroubleshootingLink(nullptr)
    , m_hRestartAsAdmin(nullptr)
    , m_hSettings(nullptr)
    , m_hUpdateStatus(nullptr)
    , m_hUpdateButton(nullptr)
    , m_hErrorBrush(CreateSolidBrush(RGB(255, 214, 224)))
    , m_Selected(-1)
{
}

InjectWindow::~InjectWindow()
{
    if (m_hErrorBrush) {
        DeleteObject(m_hErrorBrush);
    }
}

void InjectWindow::SetInitialScanResult(InjectScanResult result)
{
    m_PendingScanResult = std::move(result);
}

bool InjectWindow::Create()
{
    if (!m_PendingScanResult) {
        m_PendingScanResult = RunScan();
    }
    SetWindowName(L"GWToolbox - Launch");
    SetWindowDimension(305, 165);
    return Window::Create();
}

bool InjectWindow::GetSelected(size_t* index) const
{
    if (m_Selected >= 0) {
        *index = static_cast<size_t>(m_Selected);
        return true;
    }
    return false;
}

Process InjectWindow::TakeSelectedProcess(const size_t index)
{
    return std::move(m_ScanResult.m_Processes[index].m_Process);
}

LRESULT InjectWindow::WndProc(HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    switch (uMsg) {
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

        case WM_CTLCOLORSTATIC:
            if (reinterpret_cast<HWND>(lParam) == m_hErrorText) {
                const auto hDc = reinterpret_cast<HDC>(wParam);
                SetBkColor(hDc, RGB(255, 214, 224));
                SetTextColor(hDc, RGB(120, 0, 20));
                return reinterpret_cast<LRESULT>(m_hErrorBrush);
            }
            break;

        case WM_KEYUP:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hWnd);
            }
            else if (wParam == VK_RETURN) {
                if (m_ScanResult.m_Found) {
                    m_Selected = SendMessageW(m_hCharacters, CB_GETCURSEL, 0, 0);
                    DestroyWindow(hWnd);
                }
                else {
                    Rescan();
                }
            }
            break;

        case WM_DPICHANGED:
            OnDpiChanged(wParam, lParam);
            break;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

void InjectWindow::OnCreate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    ApplyDpiScaling(hWnd);

    if (m_PendingScanResult) {
        m_ScanResult = std::move(*m_PendingScanResult);
        m_PendingScanResult.reset();
    }

    m_UpdateChecker.Start(!settings.noexecheck, !settings.noupdate);

    BuildControls(hWnd);
}

void InjectWindow::BuildControls(HWND hWnd)
{
    // A Retry rebuild starts from a blank slate rather than patching the previous state in place.
    for (const auto handle :
         {&m_hCharacters, &m_hLaunchButton, &m_hErrorText, &m_hRetryButton, &m_hTroubleshootingLink, &m_hRestartAsAdmin, &m_hSettings, &m_hUpdateStatus, &m_hUpdateButton}) {
        if (*handle) {
            DestroyWindow(*handle);
            *handle = nullptr;
        }
    }

    if (m_ScanResult.m_Found) {
        const HWND hGroupBox = CreateWindowW(WC_BUTTONW, L"Select Character", WS_VISIBLE | WS_CHILD | BS_GROUPBOX, Scale(10), Scale(5), Scale(270), Scale(55), hWnd, nullptr, m_hInstance, nullptr);
        SendMessageW(hGroupBox, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));

        m_hCharacters = CreateWindowW(
            WC_COMBOBOXW, L"", WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST,
            Scale(20),  // x
            Scale(25),  // y
            Scale(155), // width
            Scale(25),  // height when dropped only - Windows fixes the closed-box height from the font, ignoring this
            hWnd, nullptr, m_hInstance, nullptr
        );
        SendMessageW(m_hCharacters, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));

        // Read back the combo box's real rendered rect so the button matches it exactly, since its closed height isn't the one requested above.
        RECT comboRect;
        GetWindowRect(m_hCharacters, &comboRect);
        MapWindowPoints(nullptr, hWnd, reinterpret_cast<LPPOINT>(&comboRect), 2);

        m_hLaunchButton = CreateWindowW(
            WC_BUTTONW, L"Launch", WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON,
            Scale(180),                       // x
            comboRect.top,                    // y
            Scale(90),                        // width
            comboRect.bottom - comboRect.top, // height
            hWnd, nullptr, m_hInstance, nullptr
        );
        SendMessageW(m_hLaunchButton, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));

        for (size_t i = 0; i < m_ScanResult.m_Processes.size(); i++) {
            const InjectProcess& process = m_ScanResult.m_Processes[i];

            wchar_t buffer[128];
            StrCopyW(buffer, _countof(buffer), process.m_Charname.c_str());
            if (process.m_Injected) {
                StrAppendW(buffer, _countof(buffer), L" (injected)");
            }

            SendMessageW(m_hCharacters, CB_ADDSTRING, 0, (LPARAM)buffer);
        }

        size_t top_most_index;
        if (FindTopMostProcess(m_ScanResult.m_Processes, &top_most_index)) {
            SendMessageW(m_hCharacters, CB_SETCURSEL, top_most_index, 0);
        }
        else {
            SendMessageW(m_hCharacters, CB_SETCURSEL, 0, 0);
        }
    }
    else {
        m_hErrorText = CreateWindowW(WC_STATICW, m_ScanResult.m_ErrorMessage.c_str(), WS_VISIBLE | WS_CHILD | SS_LEFT, Scale(10), Scale(5), Scale(165), Scale(55), hWnd, nullptr, m_hInstance, nullptr);
        SendMessageW(m_hErrorText, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));

        m_hRetryButton = CreateWindowW(WC_BUTTONW, L"Retry", WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON, Scale(190), Scale(15), Scale(90), Scale(35), hWnd, nullptr, m_hInstance, nullptr);
        SendMessageW(m_hRetryButton, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));
    }

    if (m_ScanResult.m_TroubleshootingUrl) {
        m_hTroubleshootingLink = CreateWindowW(WC_BUTTONW, L"Open troubleshooting guide", WS_VISIBLE | WS_CHILD | WS_TABSTOP, Scale(10), Scale(95), Scale(165), Scale(25), hWnd, nullptr, m_hInstance, nullptr);
        SendMessageW(m_hTroubleshootingLink, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));
    }
    else if (!IsRunningAsAdmin()) {
        m_hRestartAsAdmin = CreateWindowW(WC_BUTTONW, L"Can't find your character?", WS_VISIBLE | WS_CHILD | WS_TABSTOP, Scale(10), Scale(95), Scale(165), Scale(25), hWnd, nullptr, m_hInstance, nullptr);
        SendMessageW(m_hRestartAsAdmin, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));
        Button_SetElevationRequiredState(m_hRestartAsAdmin, TRUE);
    }

    // Update status row: status text and (when available) an Update button, both to the left of Settings.
    m_hUpdateStatus = CreateWindowW(WC_STATICW, L"", WS_VISIBLE | WS_CHILD | SS_LEFT, Scale(10), Scale(65), Scale(115), Scale(25), hWnd, nullptr, m_hInstance, nullptr);
    SendMessageW(m_hUpdateStatus, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));

    m_hUpdateButton = CreateWindowW(WC_BUTTONW, L"Update", WS_CHILD | WS_TABSTOP, Scale(130), Scale(65), Scale(60), Scale(25), hWnd, nullptr, m_hInstance, nullptr);
    SendMessageW(m_hUpdateButton, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));

    m_hSettings = CreateWindowW(WC_BUTTONW, L"Settings...", WS_VISIBLE | WS_CHILD | WS_TABSTOP, Scale(200), Scale(65), Scale(80), Scale(25), hWnd, nullptr, m_hInstance, nullptr);
    SendMessageW(m_hSettings, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));

    RefreshUpdateStatus();
}

void InjectWindow::Rescan()
{
    m_ScanResult = RunScan();
    BuildControls(m_hWnd);
}

void InjectWindow::Tick()
{
    if (m_UpdateChecker.Poll()) {
        RefreshUpdateStatus();
    }
}

void InjectWindow::RefreshUpdateStatus()
{
    if (!m_hUpdateStatus) return; // controls not built yet

    if (m_UpdateChecker.IsAnyUpdateAvailable()) {
        SetWindowTextW(m_hUpdateStatus, L"Updates available!");
        ShowWindow(m_hUpdateButton, SW_SHOW);
        return;
    }

    ShowWindow(m_hUpdateButton, SW_HIDE);
    SetWindowTextW(m_hUpdateStatus, m_UpdateChecker.IsChecking() ? L"Checking..." : L"");
}

void InjectWindow::SetControlsEnabled(const bool enabled)
{
    for (const HWND handle : {m_hCharacters, m_hLaunchButton, m_hRetryButton, m_hTroubleshootingLink, m_hRestartAsAdmin, m_hSettings, m_hUpdateButton}) {
        if (handle) EnableWindow(handle, enabled);
    }
}

void InjectWindow::OnCommand(HWND hWnd, const LONG ControlId, LONG NotificationCode)
{
    if (hWnd == m_hLaunchButton && ControlId == STN_CLICKED) {
        m_Selected = SendMessageW(m_hCharacters, CB_GETCURSEL, 0, 0);
        DestroyWindow(m_hWnd);
    }
    else if (hWnd == m_hRetryButton && ControlId == STN_CLICKED) {
        Rescan();
    }
    else if (hWnd == m_hRestartAsAdmin && ControlId == STN_CLICKED) {
        RestartWithSameArgs(true);
    }
    else if (hWnd == m_hTroubleshootingLink && ControlId == STN_CLICKED) {
        ShellExecuteW(nullptr, L"open", m_ScanResult.m_TroubleshootingUrl, nullptr, nullptr, SW_SHOWNORMAL);
    }
    else if (hWnd == m_hUpdateButton && ControlId == STN_CLICKED) {
        SetControlsEnabled(false);
        std::wstring error;
        if (!m_UpdateChecker.ApplyUpdates(error)) {
            fprintf(stderr, "ApplyUpdates failed: %ls\n", error.c_str());
        }
        SetControlsEnabled(true);
        RefreshUpdateStatus();
    }
    else if (hWnd == m_hSettings && ControlId == STN_CLICKED) {
        m_SettingsWindow.Create();
    }
}

static FARPROC GetLoadLibrary()
{
    const auto Kernel32 = GetModuleHandleW(L"Kernel32.dll");
    if (Kernel32 == nullptr) {
        fprintf(stderr, "GetModuleHandleW failed (%lu)\n", GetLastError());
        return nullptr;
    }

    const auto pLoadLibraryW = GetProcAddress(Kernel32, "LoadLibraryW");
    if (pLoadLibraryW == nullptr) {
        fprintf(stderr, "GetProcAddress failed (%lu)\n", GetLastError());
        return nullptr;
    }

    return pLoadLibraryW;
}

bool InjectRemoteThread(const Process* process, const LPCWSTR ImagePath, LPDWORD lpExitCode)
{
    *lpExitCode = 0;

    const auto ProcessHandle = process->GetHandle();
    if (ProcessHandle == nullptr) {
        fprintf(stderr, "Can't inject a dll in a process which is not open\n");
        return FALSE;
    }

    const LPVOID pLoadLibraryW = GetLoadLibrary();
    if (pLoadLibraryW == nullptr) {
        return FALSE;
    }

    const size_t ImagePathLength = wcslen(ImagePath);
    const size_t ImagePathSize = ImagePathLength * 2 + 2;

    const LPVOID ImagePathAddress = VirtualAllocEx(ProcessHandle, nullptr, ImagePathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (ImagePathAddress == nullptr) {
        fprintf(stderr, "VirtualAllocEx failed (%lu)\n", GetLastError());
        return FALSE;
    }

    SIZE_T BytesWritten;
    BOOL Success = WriteProcessMemory(ProcessHandle, ImagePathAddress, ImagePath, ImagePathSize, &BytesWritten);

    if (!Success || ImagePathSize != BytesWritten) {
        fprintf(stderr, "WriteProcessMemory failed (%lu)\n", GetLastError());
        VirtualFreeEx(ProcessHandle, ImagePathAddress, 0, MEM_RELEASE);
        return FALSE;
    }

    DWORD ThreadId;
    const HANDLE hThread = CreateRemoteThreadEx(ProcessHandle, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(pLoadLibraryW), ImagePathAddress, 0, nullptr, &ThreadId);

    if (hThread == nullptr) {
        fprintf(stderr, "CreateRemoteThreadEx failed (%lu)\n", GetLastError());
        return FALSE;
    }

    const DWORD Reason = WaitForSingleObject(hThread, INFINITE);
    if (Reason != WAIT_OBJECT_0) {
        fprintf(stderr, "WaitForSingleObject failed {reason: %lu, error: %lu}\n", Reason, GetLastError());
        CloseHandle(hThread);
        return FALSE;
    }

    VirtualFreeEx(ProcessHandle, ImagePathAddress, 0, MEM_RELEASE);

    DWORD ExitCode;
    Success = GetExitCodeThread(hThread, &ExitCode);
    CloseHandle(hThread);

    if (Success == FALSE) {
        fprintf(stderr, "GetExitCodeThread failed (%lu)\n", GetLastError());
        return FALSE;
    }

    *lpExitCode = ExitCode;
    return TRUE;
}
