#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Options.h"

Options options;

void PrintUsage(bool terminate)
{
    fprintf(stderr,
            "Usage: [options]\n\n"

            "    --version                  Print version and exist\n"
            "    -h, --help                 Print this help\n\n"

            "    --install                  Create necessary folders and download GWToolboxdll\n"
            "    --uninstall                Remove all data used by GWToolbox\n"
            "    --reinstall                Do a fresh installation\n\n"

            "    --pid <process id>         Process id of the target in which to inject\n"
            );

    if (terminate)
        exit(0);
}

void ParseCommandLine(int argc, char *argv[])
{
    for (int i = 0; i < argc; ++i) {
        char *arg = argv[i];

        if (strcmp(arg, "--version") == 0) {
            options.version = true;
        } else if (strcmp(arg, "--install") == 0) {
            options.install = true;
        } else if (strcmp(arg, "--uninstall") == 0) {
            options.uninstall = true;
        } else if (strcmp(arg, "--reinstall") == 0) {
            options.reinstall = true;
        } else if (strcmp(arg, "--pid")) {
            fprintf(stderr, "Process id\n");
            if (++i == argc) {
                fprintf(stderr, "'--pid' must be followed by a process id\n");
                PrintUsage(true);
            }
            // @Enhancement: Replace by proper 'ParseInt' that deal with errors
            options.pid = atoi(argv[i]);
        } else if (strcmp(arg, "--help") == 0) {
            options.help = true;
        } else if (strcmp(arg, "-h") == 0) {
            options.help = true;
        } else {
            options.help = true;
        }
    }

    if (options.help)
        PrintUsage(true);

    int count = 0;
    if (options.install)
        ++count;
    if (options.uninstall)
        ++count;
    if (options.reinstall)
        ++count;

    if (count > 0) {
        printf("You can only use one of '--install', '--uinstall' and '--reinstall'\n");
        PrintUsage(true);
    }
}
