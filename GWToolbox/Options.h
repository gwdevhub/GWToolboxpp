#pragma once

struct Options
{
    bool help;
    bool version;
    bool quiet;
    bool install;
    bool uninstall;
    bool reinstall;
    bool asadmin;
    bool noupdate;
    int  pid;
};

extern Options options;

void PrintUsage(bool terminate);

void ParseRegOptions();
void ParseCommandLine();

bool IsRunningAsAdmin();
bool CreateProcessAsAdmin(const wchar_t *path, const wchar_t *args, const wchar_t *workdir);
bool RestartAsAdmin(const wchar_t *args);
void RestartAsAdminWithSameArgs();
bool EnableDebugPrivilege();
