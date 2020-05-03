#include <stdio.h>
#include <stdlib.h>

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#ifdef NOMINMAX
# define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>

#include "Inject.h"
#include "Options.h"
#include "Process.h"

bool IsRunningAsAdmin()
{
    // Allocate and initialize a SID of the administrators group.
    PSID AdministratorsGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (!AllocateAndInitializeSid(
        &NtAuthority, 
        2, 
        SECURITY_BUILTIN_DOMAIN_RID, 
        DOMAIN_ALIAS_RID_ADMINS, 
        0, 0, 0, 0, 0, 0, 
        &AdministratorsGroup))
    {
        fprintf(stderr, "AllocateAndInitializeSid failed: %lu\n", GetLastError());
        return false;
    }

    // Determine whether the SID of administrators group is enabled in 
    // the primary access token of the process.
    BOOL IsRunAsAdmin = FALSE;
    if (!CheckTokenMembership(NULL, AdministratorsGroup, &IsRunAsAdmin))
    {
        FreeSid(AdministratorsGroup);
        fprintf(stderr, "CheckTokenMembership failed: %lu\n", GetLastError());
        return false;
    }

    FreeSid(AdministratorsGroup);
    return (IsRunAsAdmin != FALSE);
}

static bool CreateProcessAsAdmin(const wchar_t *path, const wchar_t *args, const wchar_t *workdir)
{
    wchar_t command_line[1024] = L"";
    size_t n_path = wcslen(path);
    size_t n_args = wcslen(args);
    if ((n_path + n_args + 2) >= _countof(command_line))
        return false;

    wcscat_s(command_line, path);
    wcscat_s(command_line, L" ");
    wcscat_s(command_line, args);

    SHELLEXECUTEINFOW ExecInfo = {0};
    ExecInfo.cbSize = sizeof(ExecInfo);
    ExecInfo.fMask = SEE_MASK_NOASYNC;
    ExecInfo.lpVerb = L"runas";
    ExecInfo.lpFile = path;
    ExecInfo.lpParameters = args;
    ExecInfo.lpDirectory = workdir;
    ExecInfo.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&ExecInfo)) {
        fprintf(stderr, "ShellExecuteExA failed: %lu\n", GetLastError());
        return false;
    }

    return true;
}

static bool RestartAsAdmin(const wchar_t *args = L"")
{
    wchar_t path[1024];
    if (!GetModuleFileNameW(GetModuleHandleW(nullptr), path, _countof(path))) {
        fprintf(stderr, "GetModuleFileNameW failed: %lu\n", GetLastError());
        return false;
    }

    wchar_t workdir[1024];
    if (!GetCurrentDirectoryW(_countof(workdir), workdir)) {
        fprintf(stderr, "GetCurrentDirectoryW failed: %lu\n", GetLastError());
        return false;
    }

    return CreateProcessAsAdmin(path, args, workdir);
}

static bool EnableDebugPrivilege()
{
    HANDLE token;
    const DWORD flags = TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY;
    if (!OpenProcessToken(GetCurrentProcess(), flags, &token)) {
        fprintf(stderr, "OpenProcessToken failed: %lu\n", GetLastError());
        return false;
    }

    LUID luid;
    if (!LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid)) {
        CloseHandle(token);
        fprintf(stderr, "LookupPrivilegeValue failed: %lu\n", GetLastError());
        return false;
    }

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr)) {
        CloseHandle(token);
        fprintf(stderr, "AdjustTokenPrivileges failed: %lu\n", GetLastError());
        return false;   
    }

    return true;
}

int main(int argc, char *argv[])
{
    ParseCommandLine(argc - 1, argv + 1);

    if (options.version) {
        printf("GWToolbox version 3.0\n");
    }

    Process proc;
    if (options.install) {
        printf("NOT IMPLEMENTED\n");
        return 0;
    } else if (options.uninstall) {
        printf("NOT IMPLEMENTED\n");
        return 0;
    } else if (options.reinstall) {
        printf("NOT IMPLEMENTED\n");
        return 0;
    } else if (options.pid) {
        if (!proc.Open(options.pid)) {
            fprintf(stderr, "Couldn't open process %d\n", options.pid);
            return 1;
        }
    } else {
        if (!InjectWindow::AskInjectProcess(&proc)) {
            if (IsRunningAsAdmin()) {
                fprintf(stderr, "InjectWindow::AskInjectName failed\n");
                return 1;
            } else {
                wchar_t args[64];
                swprintf(args, 64, L"--pid %lu", proc.GetProcessId());
                RestartAsAdmin(args);
                return 0;
            }
        }
    }

    if (!EnableDebugPrivilege() && !IsRunningAsAdmin()) {
        wchar_t args[64];
        swprintf(args, 64, L"--pid %lu", proc.GetProcessId());
        RestartAsAdmin(args);
        return 0;
    }

    DWORD ExitCode;
    if (!InjectRemoteThread(proc, L"D:\\GWToolboxpp\\Debug\\GWToolboxdll.dll", &ExitCode)) {
        if (!IsRunningAsAdmin()) {
            wchar_t args[64];
            swprintf(args, 64, L"--pid %lu", proc.GetProcessId());
            RestartAsAdmin(args);
            return 0;
        }
    }

    return 0;
}
