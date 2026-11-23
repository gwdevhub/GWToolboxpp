#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

struct Settings {
    bool help = false;
    bool version = false;
    bool quiet = false;
    bool install = false;
    bool uninstall = false;
    bool reinstall = false;
    bool asadmin = false;
    bool noupdate = false;
    bool noexecheck = false;
    bool noinstall = false;
    bool localdll = false;
    bool status = false;
    bool scan = false;
    bool json = false;
    uint32_t pid;
};

extern Settings settings;

void PrintUsage(bool terminate);

void ParseCommandLine();

bool IsRunningAsAdmin();
bool CreateProcessInt(const wchar_t* path, const wchar_t* args, const wchar_t* workdir, bool as_admin = false);
bool Restart(const wchar_t* args, bool force_admin = false);
bool RestartAsAdmin(const wchar_t* args);
void RestartWithSameArgs(bool force_admin = false);
wchar_t* GetCommandLineWithoutProgram();
bool EnableDebugPrivilege();
