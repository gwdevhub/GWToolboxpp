#include "stdafx.h"

#include <Defender.h>
#include <Module.h>
#include <Path.h>
#include <Str.h>
#include <SystemInfo.h>

#include "Inject.h"
#include "Install.h"
#include "ScanResult.h"
#include "Settings.h"
#include "WasmInject.h"
#include "WindowsDefender.h"
#include "WindowsInject.h"
#include "Utf8.h"

namespace fs = std::filesystem;

namespace {

bool RestartAsAdminForInjection(const uint32_t target_pid)
{
    wchar_t args[64];
    swprintf(args, 64, L"/pid %u", target_pid);
    return RestartAsAdmin(args);
}

void ScanWasmSessions(std::vector<InjectProcess>& inject_processes)
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

std::vector<fs::path> GetGuildWarsExecutablePaths()
{
    std::vector<fs::path> paths;
    for (auto& process : GetGuildWarsProcesses()) {
        std::wstring path;
        if (!process.GetPath(path))
            continue;
        fs::path exe(std::move(path));
        if (std::ranges::find(paths, exe) == paths.end())
            paths.push_back(std::move(exe));
    }
    return paths;
}

InjectScanResult RunScan()
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
                           ? L"GWToolbox is for Guild Wars, not Guild Wars 2. Start Guild Wars, then click Re-scan."
                           : L"Guild Wars isn't running. Start the game, then click Re-scan.";
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
        result.m_ErrorMessage = !native_error.empty() ? native_error : L"Guild Wars isn't running. Start the game, then click Re-scan.";
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

bool InjectInstalledDllInProcess(const Process* process, std::wstring& error)
{
    std::wstring exe_filename;
    if (!PathGetExeFileName(exe_filename)) return error = L"PathGetExeFileName failed", false;

    error.clear();
    ProcessModule module;
    if (process->GetModule(&module, L"GWToolboxdll.dll")) {
        return error = L"GWToolbox is already running in this process", true;
    }

    if (!EnableDebugPrivilege() && !IsRunningAsAdmin()) {
        RestartAsAdminForInjection(process->GetProcessId());
        return true;
    }

    fs::path dllpath;
    dllpath = GetInstallationDir();
    if (dllpath.empty()) return error = L"GetInstallationDir failed", false;
    dllpath = dllpath.parent_path() / L"GWToolboxdll.dll";
    if (settings.localdll) {
        if (!PathGetProgramDirectory(dllpath)) return error = L"PathGetProgramDirectory failed", false;
        dllpath = dllpath / L"GWToolboxdll.dll";
        if (!exists(dllpath)) {
            return error = std::format(L"Application @ {} not found.\n\nEnsure your copy of {} is local to {} or run {} without the /localdll argument.", dllpath.wstring(), dllpath.filename().wstring(), exe_filename, exe_filename), false;
        }
    }

    if (!exists(dllpath)) {
        if (settings.noinstall) {
            return error = std::format(L"Application @ {} not found.\n\nYou may need to install GWToolbox by running {} without the /noinstall argument.", dllpath.wstring(), exe_filename), false;
        }
        return error =
                   std::format(L"Application @ {} not found; it may have been quarantined by anti virus software!\n\nExclude the {} directory in your anti virus settings and re-launch {}.", dllpath.wstring(), dllpath.parent_path().wstring(), exe_filename),
               false;
    }

    DWORD ExitCode;
    if (!InjectRemoteThread(process, dllpath.wstring().c_str(), &ExitCode)) return error = std::format(L"InjectRemoteThread failed (ExitCode: {})", ExitCode), false;
    if (!exists(dllpath))
        return error = std::format(
                   L"Application @ {} did exist, but now it doesn't; it may have been quarantined by anti virus software!\n\nExclude the {} directory in your anti virus settings and re-launch {}.", dllpath.wstring(), dllpath.parent_path().wstring(),
                   exe_filename
               ),
               false;
    if (!process->GetModule(&module, L"GWToolboxdll.dll")) {
        std::wstring detail;
        if (FindRecentDefenderBlock(L"GWToolboxdll.dll", 30, detail))
            return error = std::format(L"Windows Defender blocked GWToolbox from loading:\n\n{}\n\nAdd an exclusion for the {} directory in Windows Security and re-launch {}.", detail, dllpath.parent_path().wstring(), exe_filename), false;
        return error = std::format(
                   L"Application @ {} failed to inject; it may have been quarantined by anti virus software!\n\nExclude the {} directory in your anti virus settings and re-launch {}.", dllpath.wstring(), dllpath.parent_path().wstring(), exe_filename
               ),
               false;
    }

    return true;
}

bool InjectWasmModIntoSession(const WasmSession& session, std::wstring& error)
{
    std::wstring exe_filename;
    if (!PathGetExeFileName(exe_filename)) return error = L"PathGetExeFileName failed", false;

    fs::path gwmod_path = GetInstallationDir();
    if (gwmod_path.empty()) return error = L"GetInstallationDir failed", false;
    gwmod_path = gwmod_path.parent_path() / WASM_GWMOD_FILENAME;
    if (settings.localdll) {
        if (!PathGetProgramDirectory(gwmod_path)) return error = L"PathGetProgramDirectory failed", false;
        gwmod_path = gwmod_path / WASM_GWMOD_FILENAME;
        if (!exists(gwmod_path)) {
            return error = std::format(L"Application @ {} not found.\n\nEnsure your copy of {} is local to {} or run {} without the /localdll argument.", gwmod_path.wstring(), gwmod_path.filename().wstring(), exe_filename, exe_filename), false;
        }
    }

    if (!exists(gwmod_path)) {
        return error = std::format(L"Application @ {} not found.\n\nThe wasm build of GWToolbox may not be installed alongside the native one.", gwmod_path.wstring()), false;
    }

    std::vector<std::string> log_lines;
    const bool ok = InjectWasmMod(session, gwmod_path, log_lines, error);
    for (const auto& line : log_lines) {
        fprintf(stderr, "  %s\n", line.c_str());
    }
    if (!ok && !log_lines.empty()) {
        error += L"\n\ngw_in_browser reported:\n";
        for (const auto& line : log_lines) {
            error += L"  " + FromUtf8(line) + L"\n";
        }
    }
    return ok;
}

bool SetProcessForeground(const Process* process)
{
    HWND hWndIt = GetTopWindow(nullptr);
    if (hWndIt == nullptr) {
        fprintf(stderr, "GetTopWindow failed (%lu)\n", GetLastError());
        return false;
    }

    const DWORD ProcessId = process->GetProcessId();

    while (hWndIt != nullptr) {
        DWORD WindowPid;
        if (GetWindowThreadProcessId(hWndIt, &WindowPid) == 0) {
            fprintf(stderr, "GetWindowThreadProcessId returned 0\n");
            continue;
        }

        if (WindowPid == ProcessId) {
            SetForegroundWindow(hWndIt);
            return true;
        }

        hWndIt = GetWindow(hWndIt, GW_HWNDNEXT);
    }

    return false;
}

InjectOutcome injectTarget(const ScanTarget& target)
{
    if (target.kind == "wasm") {
        for (const WasmSession& session : GetWasmSessions()) {
            if (session.pid == target.pid) {
                std::wstring error;
                const bool ok = InjectWasmModIntoSession(session, error);
                return {ok, ToUtf8(error)};
            }
        }
        return {false, "That gw_in_browser session is no longer running."};
    }

    Process process;
    if (!process.Open(target.pid)) {
        return {false, std::format("Couldn't open Guild Wars (process {}). Run GWToolbox as administrator and try again.", target.pid)};
    }

    if (!settings.quiet) {
        const auto install_dir = GetInstallationDir();
        fs::path exe_path;
        if (!install_dir.empty() && PathGetExeFullPath(exe_path)) {
            std::vector<fs::path> cfa_apps = GetGuildWarsExecutablePaths();
            cfa_apps.push_back(install_dir.parent_path() / L"GWToolbox.exe");
            cfa_apps.push_back(exe_path);
            EnsureDefenderReadiness(install_dir.parent_path(), cfa_apps);
        }
    }

    std::wstring error;
    if (!InjectInstalledDllInProcess(&process, error)) {
        std::wstring wine;
        std::wstring emulation;
        if (GetWineDiagnostics(wine)) {
            error += std::format(
                L"\n\nGWToolbox is running under Wine on a non-Windows host, which is not supported. The "
                L"Linux guide at <a href=\"https://www.gwtoolbox.com/docs/linux/\">gwtoolbox.com/docs/linux</a> "
                L"is provided as-is for convenience only.\n\nDetected: {}",
                wine
            );
        }
        else if (GetEmulationDiagnostics(emulation)) {
            error += std::format(
                L"\n\nGWToolbox is running under CPU emulation, which is not supported. Guild Wars and GWToolbox "
                L"are x86 software; running them on non-x86 hardware (e.g. Windows on ARM, or a VM on an Apple "
                L"Silicon Mac) is known to cause crashes, particularly with texture packs and other hooking-based "
                L"features.\n\nDetected: {}",
                emulation
            );
        }
        return {false, ToUtf8(error)};
    }

    SetProcessForeground(&process);
    return {true, std::string()};
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
