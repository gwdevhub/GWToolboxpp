#pragma once

struct Options
{
    bool quiet;
    bool version;
    bool help;
    bool install;
    bool uninstall;
    bool reinstall;
    bool asadmin;
    bool noupdate;
    int  pid;
};

extern Options options;

void PrintUsage(bool terminate);
void ParseCommandLine(int argc, char *argv[]);
