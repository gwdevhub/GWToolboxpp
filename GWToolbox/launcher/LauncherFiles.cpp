#include "LauncherUpdate.h"

#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

fs::path StagedLauncherPath(const fs::path& current)
{
    fs::path staged = ".new";
    staged += current.extension();
    return current.parent_path() / staged;
}

fs::path OldLauncherPath(const fs::path& current)
{
    fs::path old = current;
    old += ".old";
    return old;
}

bool InstallLauncherRelease(const fs::path& current, const fs::path& staged, std::string& error)
{
    const fs::path old = OldLauncherPath(current);
    std::error_code ec;
    fs::remove(old, ec);
    fs::rename(current, old, ec);
    if (ec) {
        std::error_code cleanup;
        fs::remove(staged, cleanup);
        error = "The launcher couldn't be moved aside for the update: " + ec.message();
        return false;
    }

    fs::rename(staged, current, ec);
    if (ec) {
        std::error_code restore;
        fs::rename(old, current, restore);
        fs::remove(staged, restore);
        error = "The update couldn't be put in place: " + ec.message();
        return false;
    }
    return true;
}

void RemoveLauncherLeftovers(const fs::path& current)
{
    std::error_code ec;
    const fs::path staged = StagedLauncherPath(current);
    fs::path partial = staged;
    partial += ".part";
    fs::remove(OldLauncherPath(current), ec);
    fs::remove(staged, ec);
    fs::remove(partial, ec);
}
