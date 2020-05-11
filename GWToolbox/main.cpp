#include "stdafx.h"

#include "Download.h"
#include "Inject.h"
#include "Install.h"
#include "Options.h"
#include "Path.h"
#include "Process.h"

static bool RestartAsAdminForInjection(uint32_t TargetPid)
{
    wchar_t args[64];
    swprintf(args, 64, L"/pid %lu", TargetPid);
    return RestartAsAdmin(args);
}

static bool InjectInstalledDllInProcess(Process *process)
{
    if (!EnableDebugPrivilege() && !IsRunningAsAdmin()) {
        RestartAsAdminForInjection(process->GetProcessId());
        return true;
    }

    wchar_t dllpath[MAX_PATH];
    if (!GetInstallationLocation(dllpath, MAX_PATH)) {
        fprintf(stderr, "Couldn't find installation path\n");
        return true;
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
    ParseRegOptions();
    ParseCommandLine();

    assert(options.help == false);
    if (options.version) {
        printf("GWToolbox version 3.0\n");
        return 0;
    }

    if (options.asadmin && !IsRunningAsAdmin()) {
        RestartAsAdmin(GetCommandLineWithoutProgram());
        return 0;
    }

    Process proc;
    if (options.install) {
        Install(options.quiet);
        return 0;
    } else if (options.uninstall) {
        Uninstall(options.quiet);
        return 0;
    } else if (options.reinstall) {
        // @Enhancement:
        // Uninstall shouldn't remove the existing data, that would instead be a
        // "repair" or something along those lines.
        Uninstall(options.quiet);
        Install(options.quiet);
        return 0;
    } else if (options.pid) {
        if (!proc.Open(options.pid)) {
            fprintf(stderr, "Couldn't open process %d\n", options.pid);
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
        if (options.quiet) {
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
            Install(options.quiet);
        }

    } else if (!options.noupdate) {
        if (!DownloadFiles()) {
            fprintf(stderr, "DownloadFiles failed\n");
            return false;
        }
    }

    // If we can't open with appropriate rights, we can then ask to re-open
    // as admin.
    if (!InjectWindow::AskInjectProcess(&proc)) {
        if (IsRunningAsAdmin()) {
            MessageBoxW(
                0,
                L"Couldn't find any appropriate target to start GWToolbox",
                L"GWToolbox - Error",
                0);
            fprintf(stderr, "InjectWindow::AskInjectName failed\n");
            return 0;
        } else {
            // @Enhancement:
            // Add UAC shield to the yes button
            int iRet = MessageBoxW(
                0,
                L"Couldn't find any valid process to start GWToolboxpp.\n"
                "If such process exist GWToolbox.exe may require administrator privileges.\n"
                "Do you want to restart as administrator?",
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
        fprintf(stderr, "InjectInstalledDllInProcess failed\n");
        return 1;
    }

    return 0;
}
