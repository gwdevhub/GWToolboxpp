#include "stdafx.h"

#include "Settings.h"

Settings settings;

void PrintUsage(const bool terminate)
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
            "    /noupdate                  Won't try to update\n"
            "    /noexecheck                Won't check Github for a newer GWToolbox.exe\n"
            "    /noinstall                 Won't try to install if missing\n"
            "    /localdll                  Check launcher directory for toolbox dll (or gwtoolbox.gwmod for a wasm client), won't try to install or update\n\n"

            "    /pid <process id>          Process id of the target in which to inject\n"
    );

    if (terminate) {
        exit(0);
    }
}

static bool IsOneOrZeroOf3(const bool b1, const bool b2, const bool b3)
{
    int count = 0;
    if (b1) {
        ++count;
    }
    if (b2) {
        ++count;
    }
    if (b3) {
        ++count;
    }
    return count <= 1;
}

void ParseCommandLine()
{
    int argc;
    const LPWSTR CmdLine = GetCommandLineW();
    const LPWSTR* argv = CommandLineToArgvW(CmdLine, &argc);
    if (argv == nullptr) {
        fprintf(stderr, "CommandLineToArgvW failed (%lu)\n", GetLastError());
        return;
    }

    for (int i = 1; i < argc; ++i) {
        const wchar_t* arg = argv[i];

        if (wcscmp(arg, L"/version") == 0) {
            settings.version = true;
        }
        else if (wcscmp(arg, L"/install") == 0) {
            settings.install = true;
            settings.noupdate = false;
        }
        else if (wcscmp(arg, L"/uninstall") == 0) {
            settings.uninstall = true;
        }
        else if (wcscmp(arg, L"/reinstall") == 0) {
            settings.reinstall = true;
        }
        else if (wcscmp(arg, L"/pid") == 0) {
            if (++i == argc) {
                fprintf(stderr, "'/pid' must be followed by a process id\n");
                PrintUsage(true);
            }
            const int pid = _wtoi(argv[i]);
            if (pid < 0) {
                fprintf(stderr, "Process id must be a positive integer in the range [0, 4294967295]");
                exit(0);
            }
            settings.pid = static_cast<uint32_t>(pid);
        }
        else if (wcscmp(arg, L"/asadmin") == 0) {
            settings.asadmin = true;
        }
        else if (wcscmp(arg, L"/noupdate") == 0) {
            settings.noupdate = true;
        }
        else if (wcscmp(arg, L"/noexecheck") == 0) {
            settings.noexecheck = true;
        }
        else if (wcscmp(arg, L"/help") == 0) {
            settings.help = true;
        }
        else if (wcscmp(arg, L"/noinstall") == 0) {
            settings.noinstall = true;
        }
        else if (wcscmp(arg, L"/quiet") == 0) {
            settings.quiet = true;
        }
        else if (wcscmp(arg, L"/localdll") == 0) {
            settings.localdll = true;
            settings.noupdate = true;
            settings.noinstall = true;
        }
        else if (wcscmp(arg, L"/?") == 0) {
            settings.help = true;
        }
        else {
            settings.help = true;
        }
    }

    if (settings.help) {
        PrintUsage(true);
    }

    if (!IsOneOrZeroOf3(settings.install, settings.uninstall, settings.reinstall)) {
        printf("You can only use one of '/install', '/uninstall' and '/reinstall'\n");
        PrintUsage(true);
    }
}

bool IsRunningAsAdmin()
{
    PSID AdministratorsGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (!AllocateAndInitializeSid(
        &NtAuthority,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &AdministratorsGroup)) {
        fprintf(stderr, "AllocateAndInitializeSid failed: %lu\n", GetLastError());
        return false;
    }

    BOOL IsRunAsAdmin = FALSE;
    if (!CheckTokenMembership(nullptr, AdministratorsGroup, &IsRunAsAdmin)) {
        FreeSid(AdministratorsGroup);
        fprintf(stderr, "CheckTokenMembership failed: %lu\n", GetLastError());
        return false;
    }

    FreeSid(AdministratorsGroup);
    return IsRunAsAdmin != FALSE;
}

bool CreateProcessInt(const wchar_t* path, const wchar_t* args, const wchar_t* workdir, const bool as_admin)
{
    wchar_t command_line[1024] = L"";
    const size_t n_path = wcslen(path);
    const size_t n_args = wcslen(args);
    if (n_path + n_args + 2 >= _countof(command_line)) {
        return false;
    }

    wcscat_s(command_line, path);
    wcscat_s(command_line, L" ");
    wcscat_s(command_line, args);

    SHELLEXECUTEINFOW ExecInfo = {0};
    ExecInfo.cbSize = sizeof(ExecInfo);
    ExecInfo.fMask = SEE_MASK_NOASYNC;
    if (as_admin) {
        ExecInfo.lpVerb = L"runas";
    }
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

bool Restart(const wchar_t* args, const bool force_admin)
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
    FILE* stream;
    freopen_s(&stream, "NUL", "w", stderr);

    const bool is_admin = force_admin ? true : IsRunningAsAdmin();
    CreateProcessInt(path, args, workdir, is_admin);
    ExitProcess(0);
}

bool RestartAsAdmin(const wchar_t* args)
{
    return Restart(args, true);
}

static LPWSTR ConsumeSpaces(LPWSTR Str)
{
    for (;;) {
        if (*Str != ' ') {
            return Str;
        }
        ++Str;
    }
}

static LPWSTR ConsumeArg(LPWSTR CmdLine)
{
    bool Quotes = false;

    for (;;) {
        if (*CmdLine == 0) {
            return CmdLine;
        }

        if (*CmdLine == '"') {
            if (Quotes) {
                return ConsumeSpaces(CmdLine + 1);
            }
            Quotes = true;
        }
        else if (*CmdLine == ' ' && !Quotes) {
            return ConsumeSpaces(CmdLine);
        }

        ++CmdLine;
    }
}

static wchar_t* GetCommandLineWithoutProgram()
{
    const LPWSTR CmdLine = GetCommandLineW();
    return ConsumeArg(CmdLine);
}

void RestartWithSameArgs(const bool force_admin)
{
    Restart(GetCommandLineWithoutProgram(), force_admin);
}

bool EnableDebugPrivilege()
{
    HANDLE token;
    constexpr DWORD flags = TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY;
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

SettingsWindow::SettingsWindow()
    : LauncherWindow("GWToolbox - Settings", 305, 135, IDR_SETTINGS_PAGE)
{
    handler().on("getState", [](WebViewWindow&, const Json&, const Reply& reply) {
        reply.result(Json{{"noUpdate", settings.noupdate}, {"asAdmin", settings.asadmin}});
    });
    handler().on("setOption", [](WebViewWindow&, const Json& args, const Reply& reply) {
        const auto name = args.at(0).get<std::string>();
        const auto value = args.at(1).get<bool>();
        if (name == "noUpdate") {
            settings.noupdate = value;
        }
        else if (name == "asAdmin") {
            settings.asadmin = value;
        }
        else {
            throw std::invalid_argument("Unknown setting: " + name);
        }
        reply.result(nullptr);
    });
}
