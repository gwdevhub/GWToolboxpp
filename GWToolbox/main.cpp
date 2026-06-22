#include "stdafx.h"

#include <cstdio>

#include <Defender.h>
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
    MessageBoxW(nullptr, message, L"GWToolbox - Error", 0);
}

static void ShowError(const char* message)
{
    MessageBoxA(nullptr, message, "GWToolbox - Error", 0);
}

static bool RestartAsAdminForInjection(const uint32_t target_pid)
{
    wchar_t args[64];
    swprintf(args, 64, L"/pid %u", target_pid);
    return RestartAsAdmin(args);
}

// Wine (and its forks: Proton, CrossOver, Lutris, ...) exports these from ntdll; absent on a real Windows host.
// Returns false on Windows; otherwise fills `out` with the Wine version, host kernel and launcher environment.
static bool GetWineDiagnostics(std::wstring& out)
{
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return false;
    const auto get_version = reinterpret_cast<const char*(__cdecl*)()>(GetProcAddress(ntdll, "wine_get_version"));
    if (!get_version) return false;

    const auto get_host = reinterpret_cast<void(__cdecl*)(const char**, const char**)>(GetProcAddress(ntdll, "wine_get_host_version"));
    const char* sysname = nullptr;
    const char* release = nullptr;
    if (get_host) get_host(&sysname, &release);

    wchar_t buf[256];
    swprintf(buf, _countof(buf), L"Wine %S on %S %S", get_version(), sysname ? sysname : "unknown", release ? release : "");
    out = buf;

    // Steam/Proton and Lutris sandbox the wine prefix in ways known to break injection (see the Linux guide).
    if (GetEnvironmentVariableW(L"STEAM_COMPAT_DATA_PATH", nullptr, 0) || GetEnvironmentVariableW(L"SteamAppId", nullptr, 0))
        out += L"; launched via Steam/Proton (sandboxed prefix - known to break injection)";
    else if (GetEnvironmentVariableW(L"LUTRIS_GAME_UUID", nullptr, 0) || GetEnvironmentVariableW(L"LUTRIS_PGA_DB", nullptr, 0))
        out += L"; launched via Lutris (sandboxed prefix - known to break injection)";
    return true;
}

static bool InjectInstalledDllInProcess(const Process* process, std::wstring& error)
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

    std::filesystem::path dllpath;
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
    // A previous self-update renames the old exe aside; clean it up now that it's no longer locked.
    std::filesystem::path stale_exe = log_file_path;
    stale_exe += L".old";
    DeleteFileW(stale_exe.wstring().c_str());

    log_file_path = log_file_path.parent_path() / L"GWToolbox.error.log";
    static FILE* stream;
    if (freopen_s(&stream, log_file_path.string().c_str(), "w", stderr) != 0) {
        wchar_t buf[MAX_PATH + 128];
        swprintf(buf, MAX_PATH + 128, L"Failed to open log file for writing:\n\n%s\n\nEnsure you have write permissions to this folder.", log_file_path.wstring().c_str());
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

        const int iRet = MessageBoxW(nullptr, L"GWToolbox doesn't seem to be installed, do you want to install it?", L"GWToolbox", MB_YESNO);

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
    else if (!settings.noupdate || !settings.noexecheck) {
        // One Github fetch drives both the exe self-update prompt and the dll update.
        std::wstring error;
        if (!DownloadWindow::CheckForUpdates(error)) {
            ShowError(std::format(L"Failed to download GWToolbox DLL: {}", error).c_str());
            fprintf(stderr, "DownloadWindow::CheckForUpdates failed\n");
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
            L"GWToolbox - Error", MB_OK | MB_ICONERROR | MB_TOPMOST
        );
        return 1;
    }

    if (reply == InjectReply_MemoryBlocked) {
        std::wstring message =
            L"GWToolbox found Guild Wars but couldn't read its memory to locate your character.\n\n"
            L"This is almost always anti-virus or Controlled Folder Access blocking GWToolbox. "
            L"Add an exclusion for your GWToolbox folder in Windows Security, allow GWToolbox through Controlled Folder Access, then re-launch.";
        std::wstring detail;
        if (FindRecentDefenderBlock(L"Gw.exe", 5, detail))
            message += L"\n\nWindows Defender reported:\n" + detail;
        ShowTroubleshootingError(message, L"GWToolbox - Error", Troubleshooting::CantReadMemory, MB_ICONERROR | MB_TOPMOST);
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
            L"GWToolbox - Error", MB_YESNO | MB_TOPMOST
        );

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
        const int iRet = MessageBoxW(nullptr, error_message, L"GWToolbox - Error", MB_RETRYCANCEL | MB_TOPMOST);
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
        std::wstring wine;
        if (GetWineDiagnostics(wine)) {
            error += std::format(
                L"\n\nGWToolbox is running under Wine on a non-Windows host, which is not supported. The "
                L"Linux guide at https://www.gwtoolbox.com/linux is provided as-is for convenience only.\n\nDetected: {}",
                wine
            );
            fprintf(stderr, "Wine detected: %S\n", wine.c_str());
        }
        ShowError(error.c_str());
        fprintf(stderr, "InjectInstalledDllInProcess failed\n");
        return 1;
    }

    SetProcessForeground(&proc);

    return 0;
}
