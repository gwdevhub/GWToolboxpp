#include "stdafx.h"

#include <Defender.h>
#include <Module.h>
#include <Str.h>

#include "Inject.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

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
            fprintf(stderr, "GetWindowThreadProcessId returned 0\n");
            continue;
        }

        for (size_t i = 0; i < processes.size(); ++i) {
            if (processes[i].IsWasm()) continue;
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

static void ScanWasmSessions(std::vector<InjectProcess>& inject_processes)
{
    for (WasmSession& session : GetWasmSessions()) {
        WasmStatus status;
        if (!GetWasmStatus(session, status)) {
            fprintf(stderr, "gw_in_browser session on port %u isn't answering (stale session file?)\n", session.port);
            continue;
        }
        const bool injected = WasmModIsLoaded(status.mods, WASM_GWMOD_FILENAME_A);
        std::wstring label = std::format(L"WASM client (port {})", session.port);
        if (!status.page) {
            label += L" - waiting for game window";
        }
        inject_processes.emplace_back(injected, std::move(session), std::move(label));
    }
}

static InjectScanResult RunScan()
{
    InjectScanResult result;
    std::vector<InjectProcess> inject_processes;

    std::wstring native_error;
    const wchar_t* native_troubleshooting_url = nullptr;

    std::vector<Process> processes = GetGuildWarsProcesses();

    if (processes.empty()) {
        fprintf(stderr, "Didn't find any potential process to inject GWToolbox\n");
        const bool gw2_running = !GetGuildWars2Processes().empty();
        native_error = gw2_running
                           ? L"GWToolbox is for Guild Wars, not Guild Wars 2.\nStart Guild Wars, then click Retry."
                           : L"Guild Wars isn't running.\nStart the game, then click Retry.";
    }
    else {
        uintptr_t charname_rva = 0;
        uintptr_t email_rva = 0;
        bool read_blocked = false;
        DWORD read_error = 0;

        for (int i = 0; i < processes.size(); i++) {
            const ProcessScanner scanner(&processes[i]);
            if (!scanner.IsValid()) {
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
                native_error = L"Couldn't read Guild Wars' memory to find your character.\nThis is usually antivirus or Controlled Folder Access blocking GWToolbox.";
                native_troubleshooting_url = Troubleshooting::CantReadMemory;
            }
            else {
                fprintf(stderr, "Couldn't find charname/email RVAs in any potential process\n");
                native_error = L"Couldn't locate your character name in memory.\nUpdate GWToolbox or contact the developers.";
            }
        }
        else {
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
                    wcscpy_s(charname, _countof(charname), L"<No character selected>");
                }

                const size_t charname_len = wcsnlen(charname, _countof(charname));
                std::wstring charname2(charname, charname + charname_len);

                inject_processes.emplace_back(injected, std::move(process), std::wstring(charname2));
            }

            if (inject_processes.empty()) {
                fprintf(stderr, "No process with a readable character name\n");
                native_error = IsRunningAsAdmin()
                                   ? L"Couldn't find a valid character to inject into."
                                   : L"Couldn't find a valid character.\nGWToolbox may need administrator privileges.";
            }
        }
    }

    processes.clear();

    ScanWasmSessions(inject_processes);

    if (inject_processes.empty()) {
        result.m_ErrorMessage = !native_error.empty() ? native_error : L"Guild Wars isn't running.\nStart the game, then click Retry.";
        result.m_TroubleshootingUrl = native_troubleshooting_url;
        return result;
    }

    std::ranges::sort(inject_processes, [](const InjectProcess& proc1, const InjectProcess& proc2) {
        return proc1.m_Charname < proc2.m_Charname;
    });

    result.m_Found = true;
    result.m_Processes = std::move(inject_processes);
    return result;
}

InjectReply InjectWindow::AskInjectProcess(InjectSelection* selection)
{
    InjectScanResult scan = RunScan();

    if (settings.quiet) {
        if (scan.m_Found && scan.m_Processes.size() == 1) {
            InjectProcess& only = scan.m_Processes[0];
            selection->is_wasm = only.IsWasm();
            if (selection->is_wasm) {
                selection->wasm_session = std::move(only.m_WasmSession);
            }
            else {
                selection->process = std::move(only.m_Process);
            }
            return InjectReply_Inject;
        }
        if (!scan.m_Found) {
            fprintf(stderr, "%ls\n", scan.m_ErrorMessage.c_str());
            return InjectReply_Cancel;
        }
    }

    if (!StartLauncherWindowing()) {
        return InjectReply_Cancel;
    }

    InjectWindow inject(std::move(scan));
    inject.open();
    PumpMessagesUntil([&inject] { return inject.Finished(); });

    size_t index;
    if (!inject.GetSelected(&index)) {
        fprintf(stderr, "No index selected\n");
        return InjectReply_Cancel;
    }

    *selection = inject.TakeSelected(index);
    return InjectReply_Inject;
}

InjectWindow::InjectWindow(InjectScanResult scan)
    : LauncherWindow("GWToolbox - Launch", 305, 200, IDR_INJECT_PAGE, L"LaunchWindow")
{
    ResetScan(std::move(scan));
    m_UpdateChecker.Start(!settings.noexecheck, !settings.noupdate);
    RegisterMethods();
}

bool InjectWindow::Finished() const
{
    return closed() && (!m_SettingsWindow || m_SettingsWindow->closed());
}

void InjectWindow::onClosed()
{
    LauncherWindow::onClosed();
    if (m_SettingsWindow) {
        m_SettingsWindow->close();
    }
}

bool InjectWindow::GetSelected(size_t* index) const
{
    if (m_Selected) {
        *index = *m_Selected;
        return true;
    }
    return false;
}

InjectSelection InjectWindow::TakeSelected(const size_t index)
{
    InjectProcess& picked = m_ScanResult.m_Processes[index];
    InjectSelection selection;
    selection.is_wasm = picked.IsWasm();
    if (selection.is_wasm) {
        selection.wasm_session = std::move(picked.m_WasmSession);
    }
    else {
        selection.process = std::move(picked.m_Process);
    }
    return selection;
}

void InjectWindow::RegisterMethods()
{
    handler().on("getState", [this](WebViewWindow&, const Json&, const Reply& reply) {
        m_UpdateChecker.Poll();
        reply.result(State());
    });
    handler().on("launch", [this](WebViewWindow& window, const Json& args, const Reply& reply) {
        const auto index = args.at(0).get<size_t>();
        if (!m_ScanResult.m_Found || index >= m_ScanResult.m_Processes.size()) {
            throw std::out_of_range("There's no character to launch.");
        }
        m_Selected = index;
        window.close();
        reply.result(nullptr);
    });
    handler().on("retry", [this](WebViewWindow&, const Json&, const Reply& reply) {
        ResetScan(RunScan());
        reply.result(State());
    });
    handler().on("troubleshooting", [this](WebViewWindow&, const Json&, const Reply& reply) {
        if (m_ScanResult.m_TroubleshootingUrl) {
            ShellExecuteW(nullptr, L"open", m_ScanResult.m_TroubleshootingUrl, nullptr, nullptr, SW_SHOWNORMAL);
        }
        reply.result(nullptr);
    });
    handler().on("restartAsAdmin", [](WebViewWindow&, const Json&, const Reply& reply) {
        RestartWithSameArgs(true);
        reply.result(nullptr);
    });
    handler().on("update", [this](WebViewWindow&, const Json&, const Reply& reply) {
        std::wstring error;
        if (!m_UpdateChecker.ApplyUpdates(error)) {
            fprintf(stderr, "ApplyUpdates failed: %ls\n", error.c_str());
        }
        reply.result(State());
    });
    handler().on("settings", [this](WebViewWindow&, const Json&, const Reply& reply) {
        OpenSettings();
        reply.result(nullptr);
    });
}

Json InjectWindow::State() const
{
    Json characters = Json::array();
    for (const InjectProcess& process : m_ScanResult.m_Processes) {
        characters.push_back(ToUtf8(process.m_Charname) + (process.m_Injected ? " (injected)" : ""));
    }

    const bool troubleshooting = m_ScanResult.m_TroubleshootingUrl != nullptr;
    const char* update_status = "";
    if (m_UpdateChecker.IsAnyUpdateAvailable()) {
        update_status = "Updates available!";
    }
    else if (m_UpdateChecker.IsChecking()) {
        update_status = "Checking...";
    }

    return Json{
        {"found", m_ScanResult.m_Found},
        {"error", ToUtf8(m_ScanResult.m_ErrorMessage)},
        {"characters", characters},
        {"selected", m_Preselected},
        {"troubleshooting", troubleshooting},
        {"canRestartAsAdmin", !troubleshooting && !IsRunningAsAdmin()},
        {"updateAvailable", m_UpdateChecker.IsAnyUpdateAvailable()},
        {"updateStatus", update_status},
    };
}

void InjectWindow::ResetScan(InjectScanResult scan)
{
    m_ScanResult = std::move(scan);
    m_Selected.reset();
    size_t top_most = 0;
    FindTopMostProcess(m_ScanResult.m_Processes, &top_most);
    m_Preselected = top_most;
}

void InjectWindow::OpenSettings()
{
    if (m_SettingsWindow && !m_SettingsWindow->closed()) {
        m_SettingsWindow->present();
        return;
    }
    m_SettingsWindow = std::make_unique<SettingsWindow>();
    m_SettingsWindow->open();
}

bool InjectRemoteThread(const Process* process, const LPCWSTR ImagePath, LPDWORD lpExitCode)
{
    *lpExitCode = 0;

    const auto ProcessHandle = process->GetHandle();
    if (ProcessHandle == nullptr) {
        fprintf(stderr, "Can't inject a dll in a process which is not open\n");
        return FALSE;
    }

    const auto pLoadLibraryW = GetModuleFunctionW<LPTHREAD_START_ROUTINE>(L"kernel32.dll", "LoadLibraryW");
    if (pLoadLibraryW == nullptr) {
        fprintf(stderr, "Failed to resolve LoadLibraryW (%lu)\n", GetLastError());
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
    const HANDLE hThread = CreateRemoteThreadEx(ProcessHandle, nullptr, 0, pLoadLibraryW, ImagePathAddress, 0, nullptr, &ThreadId);

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
