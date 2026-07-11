#include "stdafx.h"

#include <cstdio>

#include <Defender.h>
#include <Path.h>
#include <RestClient.h>

#include "Inject.h"
#include "Install.h"
#include "Process.h"
#include "Settings.h"
#include "WindowsDefender.h"

// Redirects stderr to a log file; never blocks the launcher on failure (e.g. CFA can block the exe's own folder before any AV exception is granted) - falls back to %TEMP%, then gives up silently.
static void OpenLogFile(const std::filesystem::path& exe_path)
{
    static FILE* stream;

    const auto primary = exe_path.parent_path() / L"GWToolbox.error.log";
    if (freopen_s(&stream, primary.string().c_str(), "w", stderr) == 0) {
        return;
    }

    wchar_t temp_dir[MAX_PATH];
    if (!GetTempPathW(_countof(temp_dir), temp_dir)) {
        return;
    }
    const auto fallback = std::filesystem::path(temp_dir) / L"GWToolbox.error.log";
    if (freopen_s(&stream, fallback.string().c_str(), "w", stderr) != 0) {
        return;
    }

    fprintf(stderr, "Couldn't open the log at '%S'; falling back to this file.\n", primary.c_str());
    fprintf(stderr, "%S\n", PathDiagnoseWritability(exe_path.parent_path()).c_str());
}

static void ShowError(const wchar_t* message)
{
    ShowMessageBoxW(nullptr, message, L"GWToolbox - Error", 0);
}

static void ShowError(const char* message)
{
    ShowMessageBoxA(nullptr, message, "GWToolbox - Error", 0);
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

// IsWow64Process2's native-machine field reveals the real host CPU even for our 32-bit process; ARM64 there means Gw.exe is running under x86 emulation. Loaded dynamically (Windows 10 1511+ only). Returns false on a native x86/x64 host.
static bool GetEmulationDiagnostics(std::wstring& out)
{
    const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32) return false;
    using IsWow64Process2_t = BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);
    const auto is_wow64_process2 = reinterpret_cast<IsWow64Process2_t>(GetProcAddress(kernel32, "IsWow64Process2"));
    if (!is_wow64_process2) return false;

    USHORT process_machine = IMAGE_FILE_MACHINE_UNKNOWN;
    USHORT native_machine = IMAGE_FILE_MACHINE_UNKNOWN;
    if (!is_wow64_process2(GetCurrentProcess(), &process_machine, &native_machine)) return false;
    if (native_machine != IMAGE_FILE_MACHINE_ARM64) return false; // AMD64/I386 hosts run x86 natively via ordinary WOW64

    out = L"ARM64 host (running via CPU emulation)";
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

// Opts in to per-monitor DPI awareness so scaled displays get real layout instead of blurry bitmap-stretching; must run before any window (incl. a MessageBox) is created.
static void EnableDpiAwareness()
{
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        const auto pSetProcessDpiAwarenessContext = reinterpret_cast<SetProcessDpiAwarenessContextFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (pSetProcessDpiAwarenessContext && pSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            return;
        }
    }

    SetProcessDPIAware(); // Vista+ fallback: system-DPI aware still beats fully unaware.
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
    EnableDpiAwareness();

    std::wstring error;
    std::filesystem::path exe_path;
    if (!PathGetExeFullPath(exe_path)) {
        ShowMessageBoxW(nullptr, L"Failed to get qualified path for logs file.", L"GWToolbox", MB_OK | MB_TOPMOST);
        return 0;
    }
    // A previous self-update renames the old exe aside; clean it up now that it's no longer locked.
    std::filesystem::path stale_exe = exe_path;
    stale_exe += L".old";
    DeleteFileW(stale_exe.wstring().c_str());

    OpenLogFile(exe_path);

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

        const int iRet = ShowMessageBoxW(nullptr, L"GWToolbox doesn't seem to be installed, do you want to install it?", L"GWToolbox", MB_YESNO);

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
    // Exe/dll update checking now happens inside InjectWindow (status row + Update button), not here.

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

    // Before injecting, make sure Windows Security won't block the DLL from writing to its Documents folder.
    if (!settings.quiet) {
        const auto install_dir = GetInstallationDir();
        if (!install_dir.empty()) {
            std::vector<std::filesystem::path> cfa_apps = GetGuildWarsExecutablePaths();
            cfa_apps.push_back(install_dir.parent_path() / L"GWToolbox.exe"); // the installed copy
            cfa_apps.push_back(exe_path);                                    // the copy actually running right now (e.g. a fresh, not-yet-installed download)
            EnsureDefenderReadiness(install_dir.parent_path(), cfa_apps);
        }
    }

    if (!InjectInstalledDllInProcess(&proc, error)) {
        std::wstring wine;
        std::wstring emulation;
        if (GetWineDiagnostics(wine)) {
            error += std::format(
                L"\n\nGWToolbox is running under Wine on a non-Windows host, which is not supported. The "
                L"Linux guide at https://www.gwtoolbox.com/linux is provided as-is for convenience only.\n\nDetected: {}",
                wine
            );
            fprintf(stderr, "Wine detected: %S\n", wine.c_str());
        }
        else if (GetEmulationDiagnostics(emulation)) {
            error += std::format(
                L"\n\nGWToolbox is running under CPU emulation, which is not supported. Guild Wars and GWToolbox "
                L"are x86 software; running them on non-x86 hardware (e.g. Windows on ARM, or a VM on an Apple "
                L"Silicon Mac) is known to cause crashes, particularly with texture packs and other hooking-based "
                L"features.\n\nDetected: {}",
                emulation
            );
            fprintf(stderr, "CPU emulation detected: %S\n", emulation.c_str());
        }
        ShowError(error.c_str());
        fprintf(stderr, "InjectInstalledDllInProcess failed\n");
        return 1;
    }

    SetProcessForeground(&proc);

    return 0;
}
