#include "stdafx.h"

#include <cstdio>

#include <Path.h>
#include <RestClient.h>

#include "Download.h"
#include "Inject.h"
#include "Install.h"
#include "Process.h"
#include "Settings.h"
#include "WindowsDefender.h"

static void ShowError(const wchar_t* message)
{
    MessageBoxW(
        nullptr,
        message,
        L"GWToolbox - Error",
        0);
}

static void ShowError(const char* message)
{
    MessageBoxA(
        nullptr,
        message,
        "GWToolbox - Error",
        0);
}

static bool RestartAsAdminForInjection(const uint32_t target_pid)
{
    wchar_t args[64];
    swprintf(args, 64, L"/pid %u", target_pid);
    return RestartAsAdmin(args);
}

static bool HotReloadDll(const std::wstring& new_dll_path)
{
    // Find all GW processes with toolbox loaded
    std::vector<Process> gw_processes;
    GetProcessesByWindowClass(gw_processes, L"ArenaNet_Dx_Window_Class");

    std::vector<DWORD> target_pids;
    for (auto& proc : gw_processes) {
        ProcessModule module;
        if (proc.GetModule(&module, L"GWToolboxdll.dll")) {
            target_pids.push_back(proc.GetProcessId());
        }
    }
    gw_processes.clear(); // Close handles so DLL can unload

    if (!target_pids.empty()) {
        // Signal all instances to unload (manual-reset event, all instances see it)
        HANDLE event = OpenEventA(EVENT_MODIFY_STATE, FALSE, "GWToolboxHotReload");
        if (!event) {
            fprintf(stderr, "Could not open GWToolboxHotReload event (error %lu)\n", GetLastError());
            return false;
        }
        if (!SetEvent(event)) {
            fprintf(stderr, "SetEvent failed (error %lu)\n", GetLastError());
            CloseHandle(event);
            return false;
        }

        // Wait for all instances to unload
        bool unloaded = false;
        for (int i = 0; i < 300; i++) { // up to 30 seconds
            Sleep(100);
            bool all_unloaded = true;
            for (const auto pid : target_pids) {
                Process proc;
                if (!proc.Open(pid, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ)) continue;
                ProcessModule check;
                if (proc.GetModule(&check, L"GWToolboxdll.dll")) {
                    all_unloaded = false;
                    break;
                }
            }
            if (all_unloaded) { unloaded = true; break; }
        }
        ResetEvent(event);
        CloseHandle(event);
        if (!unloaded) {
            fprintf(stderr, "Timeout waiting for DLLs to unload\n");
            return false;
        }
        Sleep(200); // Brief pause to ensure file handles are released
    }

    // Determine install path and copy the new DLL
    std::filesystem::path install_dll = GetInstallationDir();
    if (install_dll.empty()) {
        fprintf(stderr, "GetInstallationDir failed\n");
        return false;
    }
    install_dll = install_dll.parent_path() / L"GWToolboxdll.dll";

    std::error_code ec;
    std::filesystem::copy_file(new_dll_path, install_dll,
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        fprintf(stderr, "Failed to copy DLL: %s\n", ec.message().c_str());
        return false;
    }

    // Re-inject into all processes
    bool any_failed = false;
    for (const auto pid : target_pids) {
        Process proc;
        if (!proc.Open(pid)) {
            any_failed = true;
            continue;
        }
        DWORD exit_code;
        if (!InjectRemoteThread(&proc, install_dll.wstring().c_str(), &exit_code)) {
            fprintf(stderr, "Re-injection into process %lu failed\n", pid);
            any_failed = true;
            continue;
        }
    }

    return !any_failed;
}

static bool InjectInstalledDllInProcess(const Process* process, std::wstring& error)
{
    std::wstring exe_filename;
    if (!PathGetExeFileName(exe_filename))
        return error = L"PathGetExeFileName failed", false;

    error.clear();
    ProcessModule module;
    if (process->GetModule(&module, L"GWToolboxdll.dll")) {
        return error = L"GWToolbox is already running in this process", true;
    }

    if (!EnableDebugPrivilege() && !IsRunningAsAdmin()) {
        RestartAsAdminForInjection(process->GetProcessId());
        return true;
    }

    std::filesystem::path dllpath;
    dllpath = GetInstallationDir();
    if (dllpath.empty()) 
        return error = L"GetInstallationDir failed", false;
    dllpath = dllpath.parent_path() / L"GWToolboxdll.dll";
    if (settings.localdll) {
        if (!PathGetProgramDirectory(dllpath))
            return error = L"PathGetProgramDirectory failed", false;
        dllpath = dllpath / L"GWToolboxdll.dll";
        if (!exists(dllpath)) {
            return error = std::format(L"Application @ {} not found.\n\nEnsure your copy of {} is local to {} or run {} without the /localdll argument.", dllpath.wstring(), dllpath.filename().wstring(), exe_filename, exe_filename), false;
        }
    }

    if (!exists(dllpath)) {
        if (settings.noinstall) {
            return error = std::format(L"Application @ {} not found.\n\nYou may need to install GWToolbox by running {} without the /noinstall argument.", dllpath.wstring(), exe_filename), false;
        }
        return error = std::format(L"Application @ {} not found; it may have been quarantined by anti virus software!\n\nExclude the {} directory in your anti virus settings and re-launch {}.", dllpath.wstring(), dllpath.parent_path().wstring(),
                                   exe_filename), false;
    }

    DWORD ExitCode;
    if (!InjectRemoteThread(process, dllpath.wstring().c_str(), &ExitCode))
        return error = std::format(L"InjectRemoteThread failed (ExitCode: {})", ExitCode), false;
    if (!exists(dllpath))
        return error = std::format(L"Application @ {} did exist, but now it doesn't; it may have been quarantined by anti virus software!\n\nExclude the {} directory in your anti virus settings and re-launch {}.", dllpath.wstring(),
                                   dllpath.parent_path().wstring(), exe_filename), false;
    if (!process->GetModule(&module, L"GWToolboxdll.dll"))
        return error = std::format(L"Application @ {} failed to inject; it may have been quarantined by anti virus software!\n\nExclude the {} directory in your anti virus settings and re-launch {}.", dllpath.wstring(),
                                   dllpath.parent_path().wstring(), exe_filename), false;

    return true;
}

static bool SetProcessForeground(const Process* process)
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
            // @Cleanup:
            // Not clear whether this is the return value hold an error, so we just log.
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
#ifdef _DEBUG
int main()
#else
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
#endif
{
    std::wstring error;
    std::filesystem::path log_file_path;
    if (!PathGetExeFullPath(log_file_path)) {
        MessageBoxW(nullptr, L"Failed to get qualified path for logs file.", L"GWToolbox", MB_OK | MB_TOPMOST);
        return 0;
    }
    log_file_path = log_file_path.parent_path() / L"GWToolbox.error.log";
    static FILE* stream;
    if (freopen_s(&stream, log_file_path.string().c_str(), "w", stderr) != 0) {
        wchar_t buf[MAX_PATH + 128];
        swprintf(buf, MAX_PATH + 128,
                 L"Failed to open log file for writing:\n\n%s\n\nEnsure you have write permissions to this folder.",
                 log_file_path.wstring().c_str());
        MessageBoxW(nullptr, buf, L"GWToolbox", MB_OK | MB_TOPMOST);
        return 0;
    }

    ParseCommandLine();

    assert(settings.help == false);
    if (settings.version) {
        printf("GWToolbox version %s\n", GWTOOLBOXEXE_VERSION);
        return 0;
    }

    if (settings.asadmin && !IsRunningAsAdmin()) {
        RestartWithSameArgs(true);
    }

    AsyncRestScopeInit RestInitializer;

    Process proc;
    if (!settings.hotreload_dll.empty()) {
        if (!HotReloadDll(settings.hotreload_dll)) {
            fprintf(stderr, "Hot reload failed\n");
            return 1;
        }
        return 0;
    }
    if (settings.install) {
        Install(settings.quiet, error);
        return 0;
    }
    if (settings.uninstall) {
        Uninstall(settings.quiet, error);
        return 0;
    }
    if (settings.reinstall) {
        // @Enhancement:
        // Uninstall shouldn't remove the existing data, that would instead be a
        // "repair" or something along those lines.
        Uninstall(settings.quiet, error);
        Install(settings.quiet, error);
        return 0;
    }

    if (!IsInstalled() && !settings.noinstall) {
        if (settings.quiet) {
            ShowError(L"Can't ask to install if started with '/quiet'");
            fprintf(stderr, "Can't ask to install if started with '/quiet'\n");
            return 1;
        }

        const int iRet = MessageBoxW(
            nullptr,
            L"GWToolbox doesn't seem to be installed, do you want to install it?",
            L"GWToolbox",
            MB_YESNO);

        if (iRet == IDNO) {
            fprintf(stderr, "User doesn't want to install GWToolbox\n");
            return 1;
        }

        if (iRet == IDYES) {
            // @Cleanup: Check return value
            if (!Install(settings.quiet, error)) {
                ShowError(std::format(L"Failed to install GWToolbox:\n\n{}", error).c_str());
                fprintf(stderr, "Failed to install\n");
                return 1;
            }
        }
    }
    else if (!settings.noupdate) {
        std::wstring error;
        if (!DownloadWindow::DownloadAllFiles(error)) {
            ShowError(std::format(L"Failed to download GWToolbox DLL: {}", error).c_str());
            fprintf(stderr, "DownloadWindow::DownloadAllFiles failed\n");
            return 1;
        }
    }

    if (settings.pid) {
        if (!proc.Open(settings.pid)) {
            fprintf(stderr, "Couldn't open process %d\n", settings.pid);
            return 1;
        }

        if (!InjectInstalledDllInProcess(&proc, error)) {
            fprintf(stderr, "InjectInstalledDllInProcess failed\n");
            return 1;
        }

        SetProcessForeground(&proc);

        return 0;
    }

    // If we can't open with appropriate rights, we can then ask to re-open
    // as admin.
    const InjectReply reply = InjectWindow::AskInjectProcess(&proc);

    if (reply == InjectReply_Cancel) {
        fprintf(stderr, "InjectReply_Cancel\n");
        return 0;
    }

    if (reply == InjectReply_PatternError) {
        MessageBoxW(
            nullptr,
            L"Couldn't find character name RVA.\n"
            L"You need to update the launcher or contact the developers.",
            L"GWToolbox - Error",
            MB_OK | MB_ICONERROR | MB_TOPMOST);
        return 1;
    }

    if (reply == InjectReply_NoValidProcess) {
        if (IsRunningAsAdmin()) {
            ShowError(L"Failed to inject GWToolbox into Guild Wars\n");
            fprintf(stderr, "InjectWindow::AskInjectName failed\n");
            return 0;
        }
        // @Enhancement:
        // Add UAC shield to the yes button
        const int iRet = MessageBoxW(
            nullptr,
            L"Couldn't find any valid process to start GWToolboxpp.\n"
            L"Ensure Guild Wars is running before trying to run GWToolbox.\n"
            L"If such process exist GWToolbox.exe may require administrator privileges.\n"
            L"Do you want to restart as administrator?",
            L"GWToolbox - Error",
            MB_YESNO | MB_TOPMOST);

        if (iRet == IDNO) {
            fprintf(stderr, "User doesn't want to restart as admin\n");
            return 1;
        }

        if (iRet == IDYES) {
            RestartWithSameArgs(true);
            return 0;
        }
    }
    if (reply == InjectReply_NoProcess) {
        const auto gw2_processes = GetGuildWars2Processes();
        auto error_message = L"Couldn't find any valid process to start GWToolboxpp.\n"
            L"Ensure Guild Wars is running before trying to run GWToolbox.\n";
        if (!gw2_processes.empty()) {
            error_message = L"Couldn't find any valid process to start GWToolboxpp.\n"
                L"GWToolboxpp is for Guild Wars Reforged, NOT Guild Wars 2!\n";
        }
        const int iRet = MessageBoxW(
            nullptr, error_message,
            L"GWToolbox - Error",
            MB_RETRYCANCEL | MB_TOPMOST);
        if (iRet == IDCANCEL) {
            fprintf(stderr, "User doesn't want to retry\n");
            return 1;
        }

        if (iRet == IDRETRY) {
            RestartWithSameArgs();
            return 0;
        }
    }

    if (!InjectInstalledDllInProcess(&proc, error)) {
        ShowError(error.c_str());
        fprintf(stderr, "InjectInstalledDllInProcess failed\n");
        return 1;
    }

    SetProcessForeground(&proc);

    return 0;
}
