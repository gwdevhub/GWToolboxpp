#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

#include "EmbeddedIcon.h"
#include "Installation.h"
#include "Launcher.h"
#include "WebViewWindow.h"

void showError(const std::string& message)
{
    std::fprintf(stderr, "%s\n", message.c_str());
}

int main(int argc, char** argv)
{
    bool quiet = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "/localdll") == 0) {
            SetLocalDllOnly(true);
        }
        else if (std::strcmp(argv[i], "/quiet") == 0) {
            quiet = true;
        }
    }

    try {
        setApplicationIcon(embeddedAppIcon());
    }
    catch (const std::exception&) {
    }

    if (quiet) {
        return InjectQuietly();
    }
    return RunLauncher();
}
