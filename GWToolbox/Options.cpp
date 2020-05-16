#include "stdafx.h"

#include "Options.h"
#include "Registry.h"

Options options;

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

void ParseRegOptions()
{
    HKEY SettingsKey;
    if (!OpenSettingsKey(&SettingsKey)) {
        fprintf(stderr, "OpenUninstallKey failed\n");
        return;
    }

    DWORD asadmin;
    if (RegReadDWORD(SettingsKey, L"asadmin", &asadmin)) {
        options.asadmin = (asadmin != 0);
    }

    DWORD noupdate;
    if (RegReadDWORD(SettingsKey, L"noupdate", &noupdate)) {
        options.noupdate = (noupdate != 0);
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
            options.version = true;
        } else if (wcscmp(arg, L"/install") == 0) {
            options.install = true;
        } else if (wcscmp(arg, L"/uninstall") == 0) {
            options.uninstall = true;
        } else if (wcscmp(arg, L"/reinstall") == 0) {
            options.reinstall = true;
        } else if (wcscmp(arg, L"/pid") == 0) {
            if (++i == argc) {
                fprintf(stderr, "'/pid' must be followed by a process id\n");
                PrintUsage(true);
            }
            // @Enhancement: Replace by proper 'ParseInt' that deal with errors
            options.pid = _wtoi(argv[i]);
        } else if (wcscmp(arg, L"/asadmin") == 0) {
            options.asadmin = true;
        } else if (wcscmp(arg, L"/noupdate") == 0) {
            options.noupdate = true;
        } else if (wcscmp(arg, L"/help") == 0) {
            options.help = true;
        } else if (wcscmp(arg, L"/?") == 0) {
            options.help = true;
        } else {
            options.help = true;
        }
    }

    if (options.help)
        PrintUsage(true);

    if (!IsOneOrZeroOf3(options.install, options.uninstall, options.reinstall)) {
        printf("You can only use one of '/install', '/uinstall' and '/reinstall'\n");
        PrintUsage(true);
    }
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

wchar_t* GetCommandLineWithoutProgram()
{
    LPWSTR CmdLine = GetCommandLineW();
    return ConsumeArg(CmdLine);
}
