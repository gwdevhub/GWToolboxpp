#include "stdafx.h"

#include "Settings.h"
#include "Registry.h"

Settings settings;

void PrintUsage(bool terminate)
{
    fprintf(stderr,
            "Usage: [options]\n\n"

            "    /?, /help                  Print this help\n"
            "    /version                   Print version and exist\n"
            "    /quiet                     Doesn't create any interaction with the user\n\n"

            "    /install                   Create necessary folders and download GWToolboxdll\n"
            "    /uninstall                 Remove all data used by GWToolbox\n"
            "    /reinstall                 Do a fresh installation\n\n"

            "    /asadmin                   GWToolbox will try to run as admin\n"
            "    /noupdate                  Won't try to update\n\n"

            "    /pid <process id>          Process id of the target in which to inject\n"
            );

    if (terminate)
        exit(0);
}

void ParseRegSettings()
{
    HKEY SettingsKey;
    if (!OpenSettingsKey(&SettingsKey)) {
        fprintf(stderr, "OpenUninstallKey failed\n");
        return;
    }

    DWORD asadmin;
    if (RegReadDWORD(SettingsKey, L"asadmin", &asadmin)) {
        settings.asadmin = (asadmin != 0);
    }

    DWORD noupdate;
    if (RegReadDWORD(SettingsKey, L"noupdate", &noupdate)) {
        settings.noupdate = (noupdate != 0);
    }

    RegCloseKey(SettingsKey);
}

static bool IsOneOrZeroOf3(bool b1, bool b2, bool b3)
{
    int count = 0;
    if (b1) ++count;
    if (b2) ++count;
    if (b3) ++count;
    return count <= 1;
}

void ParseCommandLine()
{
    int argc;
    LPWSTR CmdLine = GetCommandLineW();
    LPWSTR *argv = CommandLineToArgvW(CmdLine, &argc);
    if (argv == nullptr) {
        fprintf(stderr, "CommandLineToArgvW failed (%lu)\n", GetLastError());
        return;
    }

    for (int i = 1; i < argc; ++i) {
        wchar_t *arg = argv[i];

        if (wcscmp(arg, L"/version") == 0) {
            settings.version = true;
        } else if (wcscmp(arg, L"/install") == 0) {
            settings.install = true;
        } else if (wcscmp(arg, L"/uninstall") == 0) {
            settings.uninstall = true;
        } else if (wcscmp(arg, L"/reinstall") == 0) {
            settings.reinstall = true;
        } else if (wcscmp(arg, L"/pid") == 0) {
            if (++i == argc) {
                fprintf(stderr, "'/pid' must be followed by a process id\n");
                PrintUsage(true);
            }
            // @Enhancement: Replace by proper 'ParseInt' that deal with errors
            settings.pid = _wtoi(argv[i]);
        } else if (wcscmp(arg, L"/asadmin") == 0) {
            settings.asadmin = true;
        } else if (wcscmp(arg, L"/noupdate") == 0) {
            settings.noupdate = true;
        } else if (wcscmp(arg, L"/help") == 0) {
            settings.help = true;
        } else if (wcscmp(arg, L"/?") == 0) {
            settings.help = true;
        } else {
            settings.help = true;
        }
    }

    if (settings.help)
        PrintUsage(true);

    if (!IsOneOrZeroOf3(settings.install, settings.uninstall, settings.reinstall)) {
        printf("You can only use one of '/install', '/uinstall' and '/reinstall'\n");
        PrintUsage(true);
    }
}

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

bool CreateProcessAsAdmin(const wchar_t *path, const wchar_t *args, const wchar_t *workdir)
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

bool RestartAsAdmin(const wchar_t *args)
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

static LPWSTR ConsumeSpaces(LPWSTR Str)
{
    for (;;)
    {
        if (*Str != ' ')
            return Str;
        ++Str;
    }
    return nullptr;
}

static LPWSTR ConsumeArg(LPWSTR CmdLine)
{
    bool Quotes = false;

    for (;;)
    {
        if (*CmdLine == 0)
            return CmdLine;

        if (*CmdLine == '"') {
            if (Quotes) {
                return ConsumeSpaces(CmdLine + 1);
            }
            Quotes = true;
        } else if (*CmdLine == ' ' && !Quotes) {
            return ConsumeSpaces(CmdLine);
        }

        ++CmdLine;
    }

    assert(!"We should never reach here");
    return nullptr;
}

static wchar_t* GetCommandLineWithoutProgram()
{
    LPWSTR CmdLine = GetCommandLineW();
    return ConsumeArg(CmdLine);
}

void RestartAsAdminWithSameArgs()
{
    RestartAsAdmin(GetCommandLineWithoutProgram());
    ExitProcess(0);
}

bool EnableDebugPrivilege()
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
