#pragma once

struct Options
{
    bool version;
    bool help;
    bool install;
    bool uninstall;
    bool reinstall;
    int  pid;
};

extern Options options;

void PrintUsage(bool terminate);
void ParseCommandLine(int argc, char *argv[]);
