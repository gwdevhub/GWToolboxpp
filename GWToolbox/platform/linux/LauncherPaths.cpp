#include "LauncherPaths.h"

#include <cstdlib>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace {

std::string appImageVariable()
{
    const char* value = std::getenv("APPIMAGE");
    return value ? value : "";
}

}

bool runningFromAppImage()
{
    return !appImageVariable().empty();
}

fs::path appImageFile()
{
    const std::string value = appImageVariable();
    std::error_code ec;
    const fs::path canonical = fs::canonical(value, ec);
    return ec ? fs::absolute(value) : canonical;
}

fs::path launcherDirectory()
{
    if (runningFromAppImage()) {
        return appImageFile().parent_path();
    }
    std::error_code ec;
    return fs::read_symlink("/proc/self/exe", ec).parent_path();
}
