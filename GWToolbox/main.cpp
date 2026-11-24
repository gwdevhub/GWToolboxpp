#include "stdafx.h"

#include <cstdio>

#include <Module.h>
#include <Path.h>
#include <RestClient.h>
#include <SystemInfo.h>

#include "Cli.h"
#include "Inject.h"
#include "Install.h"
#include "Installation.h"
#include "Launcher.h"
#include "LauncherUpdate.h"
#include "Process.h"
#include "ScanResult.h"
#include "Settings.h"
#include "Utf8.h"
#include "WindowsInject.h"

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
    SetLocalDllOnly(settings.localdll);

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

    if (settings.pid) {
        return InjectProcessIdFromCommandLine(exe_path);
    }

    if (settings.quiet) {
        return InjectQuietly();
    }

    return RunLauncher();
}
