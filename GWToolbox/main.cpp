#include "stdafx.h"

#include "Download.h"
#include "Inject.h"
#include "Install.h"
#include "Path.h"
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
    swprintf(args, 64, L"/pid %lu", TargetPid);
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

    wchar_t dllpath[MAX_PATH];
    if (!GetInstallationLocation(dllpath, MAX_PATH)) {
        fprintf(stderr, "Couldn't find installation path\n");
        return false;
    }

    PathCompose(dllpath, MAX_PATH, dllpath, L"GWToolboxdll.dll");

    DWORD ExitCode;
    if (!InjectRemoteThread(process, dllpath, &ExitCode)) {
        fprintf(stderr, "InjectRemoteThread failed (ExitCode: %lu)\n", ExitCode);
        return false;
    }

    return true;
}

#if 0
int main(void)
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#endif
{
    ParseRegSettings();
    ParseCommandLine();

    assert(settings.help == false);
    if (settings.version) {
        printf("GWToolbox version 3.0\n");
        return 0;
    }

    if (settings.asadmin && !IsRunningAsAdmin()) {
        RestartAsAdminWithSameArgs();
        return 0;
    }

    Process proc;
    if (settings.install) {
        Install(settings.quiet);
        return 0;
    } else if (settings.uninstall) {
        Uninstall(settings.quiet);
        return 0;
    } else if (settings.reinstall) {
        // @Enhancement:
        // Uninstall shouldn't remove the existing data, that would instead be a
        // "repair" or something along those lines.
        Uninstall(settings.quiet);
        Install(settings.quiet);
        return 0;
    } else if (settings.pid) {
        if (!proc.Open(settings.pid)) {
            fprintf(stderr, "Couldn't open process %d\n", settings.pid);
            return 1;
        }

        if (!InjectInstalledDllInProcess(&proc)) {
            fprintf(stderr, "InjectInstalledDllInProcess failed\n");
            return 1;
        }
    }

    // if not installed
    //  if not "do you want to install GWToolbox"
    //   quit
    //  download toolbox
    // else if version is outdated
    //  if do you want to update the version
    //   show changelog?
    // Enumerate all processes
    // if no processes found
    //  GWToolbox can't find valid target process, restart as admin?
    // if processes found
    //  ask which one to inject in (If you don't see the process, restart as admin?)
    //
    // We have the process
    // Enable debug privileges or ask to restart as admin
    // Try to inject

    if (!IsInstalled()) {
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
            fprintf(stderr, "Use doesn't want to install GWToolbox\n");
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

    // If we can't open with appropriate rights, we can then ask to re-open
    // as admin.
    InjectReply reply = InjectWindow::AskInjectProcess(&proc);

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
            MB_OK | MB_ICONERROR);
        return 1;
    }

    if (reply == InjectReply_NoProcess) {
        if (IsRunningAsAdmin()) {
            ShowError(L"Failed to inject GWToolbox into Guild Wars");
            fprintf(stderr, "InjectWindow::AskInjectName failed\n");
            return 0;
        } else {
            // @Enhancement:
            // Add UAC shield to the yes button
            int iRet = MessageBoxW(
                0,
                L"Couldn't find any valid process to start GWToolboxpp.\n"
                L"If such process exist GWToolbox.exe may require administrator privileges.\n"
                L"Do you want to restart as administrator?",
                L"GWToolbox - Error",
                MB_YESNO);

            if (iRet == IDNO) {
                fprintf(stderr, "User doesn't want to restart as admin\n");
                return 1;
            }

            if (iRet == IDYES) {
                RestartAsAdminForInjection(proc.GetProcessId());
                return 0;
            }
        }
    }

    if (!InjectInstalledDllInProcess(&proc)) {
        ShowError(L"Couldn't find any appropriate target to start GWToolbox");
        fprintf(stderr, "InjectInstalledDllInProcess failed\n");
        return 1;
    }

    return 0;
}
