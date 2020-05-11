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
void ParseCommandLine();

wchar_t* GetCommandLineWithoutProgram();
