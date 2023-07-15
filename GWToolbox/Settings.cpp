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
            "    /noupdate                  Won't try to update\n"
            "    /noinstall                 Won't try to install if missing\n"
            "    /localdll                  Check launcher directory for toolbox dll, won't try to install or update\n\n"

            "    /pid <process id>          Process id of the target in which to inject\n"
            );

    if (terminate)
        exit(0);
}

void ParseRegSettings() {
    HKEY SettingsKey;
    if (!OpenSettingsKey(&SettingsKey)) {
        fprintf(stderr, "OpenSettingsKey failed\n");
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

static void WriteRegSettings() {
    HKEY SettingsKey;
    if (!OpenSettingsKey(&SettingsKey)) {
        fprintf(stderr, "OpenUninstallKey failed\n");
        return;
    }

    if (RegWriteDWORD(SettingsKey, L"asadmin", static_cast<DWORD>(settings.asadmin))) {
        fprintf(stderr, "Failed to write 'asadmin' registry key\n");
    }

    if (RegWriteDWORD(SettingsKey, L"noupdate", static_cast<DWORD>(settings.noupdate))) {
        fprintf(stderr, "Failed to write 'noupdate' registry key\n");
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
            settings.noupdate = false;
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
            int pid = _wtoi(argv[i]);
            if (pid < 0) {
                fprintf(stderr, "Process id must be a positive integer in the range [0, 4294967295]");
                exit(0);
            }
            settings.pid = static_cast<uint32_t>(pid);
        } else if (wcscmp(arg, L"/asadmin") == 0) {
            settings.asadmin = true;
        } else if (wcscmp(arg, L"/noupdate") == 0) {
            settings.noupdate = true;
        } else if (wcscmp(arg, L"/help") == 0) {
            settings.help = true;
        } else if (wcscmp(arg, L"/noinstall") == 0) {
            settings.noinstall = true;
        } else if (wcscmp(arg, L"/quiet") == 0) {
            settings.quiet = true;
        } else if (wcscmp(arg, L"/localdll") == 0) {
            settings.localdll = true;
            settings.noupdate = true;
            settings.noinstall = true;
        } else if (wcscmp(arg, L"/?") == 0) {
            settings.help = true;
        } else {
            settings.help = true;
        }
    }

    if (settings.help)
        PrintUsage(true);

    if (!IsOneOrZeroOf3(settings.install, settings.uninstall, settings.reinstall)) {
        printf("You can only use one of '/install', '/uninstall' and '/reinstall'\n");
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

bool CreateProcessInt(const wchar_t *path, const wchar_t *args, const wchar_t *workdir, bool as_admin)
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
    if(as_admin)
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
bool Restart(const wchar_t* args, bool force_admin) {
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
    bool is_admin = force_admin ? true : IsRunningAsAdmin();
    CreateProcessInt(path, args, workdir, is_admin);
    ExitProcess(0);
}

bool RestartAsAdmin(const wchar_t* args)
{
    return Restart(args, true);
}

static LPWSTR ConsumeSpaces(LPWSTR Str)
{
    for (;;)
    {
        if (*Str != ' ')
            return Str;
        ++Str;
    }
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
}

static wchar_t* GetCommandLineWithoutProgram()
{
    LPWSTR CmdLine = GetCommandLineW();
    return ConsumeArg(CmdLine);
}

void RestartWithSameArgs(bool force_admin)
{
    Restart(GetCommandLineWithoutProgram(), force_admin);
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

static void SetCheckbox(HWND hWnd, bool Checked)
{
    if (Checked) {
        SendMessageW(hWnd, BM_SETCHECK, BST_CHECKED, 0);
    } else {
        SendMessageW(hWnd, BM_SETCHECK, BST_UNCHECKED, 0);
    }
}

static bool ToggleCheckbox(HWND hWnd)
{
    LRESULT State = SendMessageW(hWnd, BM_GETCHECK, 0, 0);
    bool Checked = (State == BST_CHECKED);
    SetCheckbox(hWnd, !Checked);
    return !Checked;
}

bool SettingsWindow::Create()
{
    SetWindowName(L"GWToolbox - Settings");
    SetWindowDimension(305, 135);
    return Window::Create();
}

LRESULT SettingsWindow::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        OnCreate(hWnd, uMsg, wParam, lParam);
        break;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        SignalStop();
        break;

    case WM_COMMAND:
        OnCommand(reinterpret_cast<HWND>(lParam), LOWORD(wParam), HIWORD(wParam));
        break;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

void SettingsWindow::OnCreate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    m_hNoUpdate = CreateWindowW(
        WC_BUTTONW,
        L"Never check for update",
        WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_CHECKBOX,
        10,
        10,
        150,
        15,
        hWnd,
        nullptr,
        m_hInstance,
        nullptr);
    SendMessageW(m_hNoUpdate, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));

    m_hStartAsAdmin = CreateWindowW(
        WC_BUTTONW,
        L"Always start as admin",
        WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_CHECKBOX,
        10,
        30,
        150,
        15,
        hWnd,
        nullptr,
        m_hInstance,
        nullptr);
    SendMessageW(m_hStartAsAdmin, WM_SETFONT, (WPARAM)m_hFont, MAKELPARAM(TRUE, 0));

    SetCheckbox(m_hNoUpdate, settings.noupdate);
    SetCheckbox(m_hStartAsAdmin, settings.asadmin);
}

void SettingsWindow::OnCommand(HWND hWnd, LONG ControlId, LONG NotificateCode)
{
    UNREFERENCED_PARAMETER(ControlId);
    UNREFERENCED_PARAMETER(NotificateCode);

    if (hWnd == m_hNoUpdate) {
        settings.noupdate = ToggleCheckbox(m_hNoUpdate);
        WriteRegSettings();
    } else if (hWnd == m_hStartAsAdmin) {
        settings.asadmin = ToggleCheckbox(m_hStartAsAdmin);
        WriteRegSettings();
    }
}
