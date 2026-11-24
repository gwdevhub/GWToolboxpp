#include "stdafx.h"

#include <cstdio>

#include <Module.h>
#include <Path.h>
#include <RestClient.h>
#include <SystemInfo.h>

#include "Cli.h"
#include "Inject.h"
#include "Install.h"
#include "Launcher.h"
#include "LauncherUpdate.h"
#include "Process.h"
#include "ScanResult.h"
#include "Settings.h"
#include "Utf8.h"
#include "WindowsInject.h"

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

    fprintf(stderr, "Couldn't open the log at '%ls'; falling back to this file.\n", primary.c_str());
    fprintf(stderr, "%ls\n", PathDiagnoseWritability(exe_path.parent_path()).c_str());
}

void showError(const std::string& message)
{
    fprintf(stderr, "%s\n", message.c_str());
    ShowMessageBoxA(nullptr, message.c_str(), "GWToolbox - Error", MB_OK | MB_ICONERROR);
}

static void ShowError(const wchar_t* message)
{
    ShowMessageBoxWithLinksW(nullptr, message, L"GWToolbox - Error");
}

static void ShowError(const char* message)
{
    ShowMessageBoxA(nullptr, message, "GWToolbox - Error", 0);
}

// Opts in to per-monitor DPI awareness so scaled displays get real layout instead of blurry bitmap-stretching; must run before any window (incl. a MessageBox) is created.
static void EnableDpiAwareness()
{
    using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    const auto pSetProcessDpiAwarenessContext = GetModuleFunctionW<SetProcessDpiAwarenessContextFn>(L"user32.dll", "SetProcessDpiAwarenessContext");
    if (pSetProcessDpiAwarenessContext && pSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        return;
    }

    SetProcessDPIAware();
}

static int InjectProcessIdFromCommandLine(const std::filesystem::path& exe_path)
{
    Process process;
    if (!process.Open(settings.pid)) {
        fprintf(stderr, "Couldn't open process %d\n", settings.pid);
        if (settings.json)
            PrintInjectResult(false, std::format(L"Couldn't open process {}", settings.pid));
        return 1;
    }

    std::wstring error;
    if (!InjectInstalledDllInProcess(&process, error)) {
        fprintf(stderr, "InjectInstalledDllInProcess failed\n");
        if (settings.json)
            PrintInjectResult(false, error);
        return 1;
    }
    if (settings.json)
        PrintInjectResult(true, error);

    SetProcessForeground(&process);
    return 0;
}

static int InjectQuietly()
{
    const ScanResult scan = ScanGuildWars();
    if (scan.targets.size() == 1) {
        const InjectOutcome outcome = injectTarget(scan.targets.front());
        if (outcome.ok) {
            return 0;
        }
        fprintf(stderr, "injectTarget failed: %s\n", outcome.message.c_str());
        return RunLauncher(outcome.message);
    }
    if (scan.targets.empty()) {
        const std::string message = scan.errors.empty() ? std::string() : scan.errors.front().message;
        fprintf(stderr, "%s\n", message.empty() ? "Guild Wars isn't running." : message.c_str());
        return RunLauncher(message);
    }
    return RunLauncher();
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
    RemoveLauncherLeftovers(exe_path);

    OpenLogFile(exe_path);
    setvbuf(stderr, nullptr, _IONBF, 0);

    ParseCommandLine();

    assert(settings.help == false);
    if (settings.version) {
        printf("GWToolbox version %s\n", GWTOOLBOXEXE_VERSION);
        return 0;
    }

    if (settings.status) {
        return PrintInstallStatus();
    }

    if (settings.scan) {
        return PrintScanResult();
    }

    if (settings.asadmin && !IsRunningAsAdmin()) {
        RestartWithSameArgs(true);
    }

    AsyncRestScopeInit RestInitializer;

    if (settings.install) {
        Install(settings.quiet, error);
        return 0;
    }
    if (settings.uninstall) {
        Uninstall(settings.quiet, error);
        return 0;
    }
    if (settings.reinstall) {
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
            if (!Install(settings.quiet, error)) {
                ShowError(std::format(L"Failed to install GWToolbox:\n\n{}", error).c_str());
                fprintf(stderr, "Failed to install\n");
                return 1;
            }
        }
    }

    if (settings.pid) {
        return InjectProcessIdFromCommandLine(exe_path);
    }

    if (settings.quiet) {
        return InjectQuietly();
    }

    return RunLauncher();
}
