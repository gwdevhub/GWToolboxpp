#include <cstdio>
#include <exception>
#include <string>

#include "EmbeddedIcon.h"
#include "Launcher.h"
#include "WebViewWindow.h"

void showError(const std::string& message)
{
    std::fprintf(stderr, "%s\n", message.c_str());
}

int main()
{
    try {
        setApplicationIcon(embeddedAppIcon());
    }
    catch (const std::exception&) {
    }
    return RunLauncher();
}
