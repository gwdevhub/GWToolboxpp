#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

#include "EmbeddedIcon.h"
#include "Inject.h"
#include "Installation.h"
#include "Launcher.h"
#include "ScanResult.h"
#include "WebViewWindow.h"

void showError(const std::string& message)
{
    std::fprintf(stderr, "%s\n", message.c_str());
}

namespace {

int InjectQuietly()
{
    const ScanResult scan = ScanGuildWars();
    if (scan.targets.size() == 1) {
        const InjectOutcome outcome = injectTarget(scan.targets.front());
        if (outcome.ok) {
            return 0;
        }
        std::fprintf(stderr, "injectTarget failed: %s\n", outcome.message.c_str());
        return RunLauncher(outcome.message);
    }
    if (scan.targets.empty()) {
        const std::string message = scan.errors.empty() ? std::string() : scan.errors.front().message;
        std::fprintf(stderr, "%s\n", message.empty() ? "Guild Wars isn't running." : message.c_str());
        return RunLauncher(message);
    }
    return RunLauncher();
}

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
