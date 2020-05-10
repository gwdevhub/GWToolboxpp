#include <stdio.h>
#include <stdlib.h>

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#ifdef NOMINMAX
# define NOMINMAX
#endif
#include <Windows.h>

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

int main(int argc, char *argv[])
{
    ParseCommandLine(argc - 1, argv + 1);

    if (options.version) {
        printf("GWToolbox version 3.0\n");
        return 0;
    }

    if (options.asadmin && !IsRunningAsAdmin()) {
        RestartAsAdmin(GetCommandLineW());
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
    } else {
        // @Enhancement:
        // If not Guild Wars is open AskInjectProcess will return false.
        // We could deal with it, by recording Guild Wars process regardless
        // if we can open them with the appropriate rights.
        //
        // If we can't open with appropriate rights, we can then ask to re-open
        // as admin.
        if (!InjectWindow::AskInjectProcess(&proc)) {
            if (IsRunningAsAdmin()) {
                MessageBoxW(0, L"Couldn't find any appropriate target to start GWToolbox", L"Error", 0);
                fprintf(stderr, "InjectWindow::AskInjectName failed\n");
                return 1;
            } else {
                static const wchar_t message[] = L"Couldn't find any processes that could be valid target to start GWToolboxpp\n"
                    "If such process exist (typically Gw.exe) It could be because GWToolbox.exe"
                    "was not started with sufficient rights\n";
                int Button = MessageBoxW(0, message, L"Error", MB_RETRYCANCEL);
                if (Button == IDRETRY) {
                    RestartAsAdminForInjection(proc.GetProcessId());
                }
                return 0;
            }
        }
    }

    if (!EnableDebugPrivilege() && !IsRunningAsAdmin()) {
        RestartAsAdminForInjection(proc.GetProcessId());
        return 0;
    }

    wchar_t dllpath[MAX_PATH];
    if (!GetInstallationLocation(dllpath, MAX_PATH)) {
        fprintf(stderr, "Couldn't find installation path\n");
        return 0;
    }

    PathCompose(dllpath, MAX_PATH, dllpath, L"GWToolboxdll.dll");

    DWORD ExitCode;
    if (!InjectRemoteThread(proc, dllpath, &ExitCode)) {
        // @Enhancement:
        // Get the reason why we couldn't inject.
    }

    return 0;
}
