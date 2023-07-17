#include "stdafx.h"

#include <cstdio>

#include <Path.h>
#include <RestClient.h>

#include "Download.h"
#include "Inject.h"
#include "Install.h"
#include "Process.h"
#include "Settings.h"

static void ShowError(const wchar_t* message) {
    MessageBoxW(
        0,
        message,
        L"GWToolbox - Error",
        0);
}

static bool RestartAsAdminForInjection(uint32_t TargetPid)
{
    wchar_t args[64];
    swprintf(args, 64, L"/pid %u", TargetPid);
    return RestartAsAdmin(args);
}

static bool InjectInstalledDllInProcess(Process *process)
{
    ProcessModule module;
    if (process->GetModule(&module, L"GWToolboxdll.dll")) {
        MessageBoxW(0, L"GWToolbox is already running in this process", L"GWToolbox", 0);
        return true;
    }

    if (!EnableDebugPrivilege() && !IsRunningAsAdmin()) {
        RestartAsAdminForInjection(process->GetProcessId());
        return true;
    }

    std::filesystem::path dllpath;
    if (!PathGetProgramDirectory(dllpath)) {
        if (settings.localdll) {
            return false;
        }
    }
    const std::filesystem::path localdll = dllpath / L"GWToolboxdll.dll";
    if (std::filesystem::exists(localdll)) {
        dllpath = localdll;
    } else if (settings.localdll) {
        return false;
    }
    else if (!PathGetDocumentsPath(dllpath, L"GWToolboxpp\\GWToolboxdll.dll")) {
        fprintf(stderr, "Couldn't find installation path\n");
        return false;
    }

    if (!std::filesystem::exists(dllpath)) {
        fprintf(stderr, "No GWToolboxdll.dll file exists.\n");
        return false;
    }

    DWORD ExitCode;
    if (!InjectRemoteThread(process, dllpath.wstring().c_str(), &ExitCode)) {
        fprintf(stderr, "InjectRemoteThread failed (ExitCode: %lu)\n", ExitCode);
        return false;
    }

    return true;
}

static bool SetProcessForeground(Process *process)
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
    std::filesystem::path log_file_path;
    if (!PathGetExeFullPath(log_file_path)) {
        MessageBoxW(0, L"Failed to get qualified path for logs file.", L"GWToolbox", MB_OK | MB_TOPMOST);
        return 0;
    }
    log_file_path = log_file_path.parent_path() / L"GWToolbox.error.log";
    if (!freopen(log_file_path.string().c_str(), "w", stderr)) {
        wchar_t buf[MAX_PATH + 128];
        swprintf(buf, MAX_PATH + 128,
            L"Failed to open log file for writing:\n\n%s\n\nEnsure you have write permissions to this folder.",
            log_file_path.wstring().c_str());
        MessageBoxW(0, buf, L"GWToolbox", MB_OK | MB_TOPMOST);
        return 0;
    }

    ParseRegSettings();
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
        Install(settings.quiet);
        return 0;
    }
    if (settings.uninstall) {
        Uninstall(settings.quiet);
        return 0;
    }
    if (settings.reinstall) {
        // @Enhancement:
        // Uninstall shouldn't remove the existing data, that would instead be a
        // "repair" or something along those lines.
        Uninstall(settings.quiet);
        Install(settings.quiet);
        return 0;
    }

    if (!IsInstalled() && !settings.noinstall) {
        if (settings.quiet) {
            ShowError(L"Can't ask to install if started with '/quiet'");
            fprintf(stderr, "Can't ask to install if started with '/quiet'\n");
            return 1;
        }

        int iRet = MessageBoxW(
            0,
            L"GWToolbox doesn't seem to be installed, do you want to install it?",
            L"GWToolbox",
            MB_YESNO);

        if (iRet == IDNO) {
            fprintf(stderr, "User doesn't want to install GWToolbox\n");
            return 1;
        }

        if (iRet == IDYES) {
            // @Cleanup: Check return value
            if (!Install(settings.quiet)) {
                fprintf(stderr, "Failed to install\n");
                return 1;
            }
        }

    } else if (!settings.noupdate) {
        if (!DownloadWindow::DownloadAllFiles()) {
            ShowError(L"Failed to download GWToolbox DLL");
            fprintf(stderr, "DownloadWindow::DownloadAllFiles failed\n");
            return 1;
        }
    }

    if (settings.pid) {
        if (!proc.Open(settings.pid)) {
            fprintf(stderr, "Couldn't open process %d\n", settings.pid);
            return 1;
        }

        if (!InjectInstalledDllInProcess(&proc)) {
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
            0,
            L"Couldn't find character name RVA.\n"
            L"You need to update the launcher or contact the developpers.",
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
        else {
            // @Enhancement:
            // Add UAC shield to the yes button
            int iRet = MessageBoxW(
                0,
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
    }
    if (reply == InjectReply_NoProcess) {
        int iRet = MessageBoxW(
            0,
            L"Couldn't find any valid process to start GWToolboxpp.\n"
            L"Ensure Guild Wars is running before trying to run GWToolbox.\n",
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

    if (!InjectInstalledDllInProcess(&proc)) {
        ShowError(L"Couldn't find any appropriate target to start GWToolbox");
        fprintf(stderr, "InjectInstalledDllInProcess failed\n");
        return 1;
    }

    SetProcessForeground(&proc);

    return 0;
}
