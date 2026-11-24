#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <spawn.h>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#include "Command.h"
#include "DllDownload.h"
#include "Download.h"
#include "LauncherPaths.h"
#include "LauncherUpdate.h"
#include "Sha256.h"

namespace fs = std::filesystem;

extern char** environ;

namespace {

constexpr auto kReleasesUrl = "https://api.github.com/repos/gwdevhub/GWToolboxpp/releases?per_page=30";
constexpr auto kCheckTimeout = std::chrono::seconds(60);
constexpr const char* kLauncherAsset = "GWToolboxLinux";
constexpr const char* kAppImageSuffix = ".AppImage";

bool isLauncherAsset(const std::string& name)
{
    if (runningFromAppImage()) {
        return name.starts_with("GWToolbox") && name.ends_with(kAppImageSuffix);
    }
    return name == kLauncherAsset;
}

std::vector<std::string> launchArguments()
{
    std::ifstream in("/proc/self/cmdline", std::ios::binary);
    const std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::string> args;
    size_t start = 0;
    while (start < data.size()) {
        const size_t end = data.find('\0', start);
        const size_t stop = end == std::string::npos ? data.size() : end;
        args.push_back(data.substr(start, stop - start));
        start = stop + 1;
    }
    if (!args.empty()) {
        args.erase(args.begin());
    }
    return args;
}

}

fs::path RunningLauncherPath()
{
    static const fs::path path = [] {
        if (runningFromAppImage()) {
            return appImageFile();
        }
        std::error_code ec;
        return fs::read_symlink("/proc/self/exe", ec);
    }();
    return path;
}

bool FetchLauncherRelease(DllRelease& release, std::string& error)
{
    const CommandOutput listing = runCommand({"curl", "-fsSL", "-H", "Accept: application/vnd.github+json", "-H", "User-Agent: GWToolbox", kReleasesUrl},
                                             currentEnvironment(), kCheckTimeout);
    if (listing.exit_code != 0) {
        error = "The GWToolbox release list couldn't be downloaded (curl exit " + std::to_string(listing.exit_code) + ").";
        return false;
    }
    const std::string missing = runningFromAppImage() ? "No GWToolbox release has an AppImage to update to yet." : std::string();
    if (!ReleaseFromListing(listing.text, isLauncherAsset, missing, release, error)) {
        return false;
    }
    if (!release.sha256.empty() && Sha256OfFile(RunningLauncherPath()) == release.sha256) {
        error.clear();
        return false;
    }
    return true;
}

bool DownloadLauncherRelease(const DllRelease& release, const fs::path& staged, const std::function<void(int)>& progress, std::string& error)
{
    if (release.sha256.empty()) {
        error = "GitHub did not provide a checksum for the update.";
        return false;
    }

    const fs::path partial = fs::path(staged.string() + ".part");
    const int exit_code = downloadWithProgress(release.url, partial, release.size, progress);

    std::error_code ec;
    if (exit_code != 0) {
        fs::remove(partial, ec);
        error = "The update couldn't be downloaded (curl exit " + std::to_string(exit_code) + ").";
        return false;
    }
    if (Sha256OfFile(partial) != release.sha256) {
        fs::remove(partial, ec);
        error = "The update's checksum didn't match the release.";
        return false;
    }
    fs::permissions(partial,
                    fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec | fs::perms::others_read | fs::perms::others_exec,
                    fs::perm_options::replace, ec);
    if (ec) {
        fs::remove(partial, ec);
        error = "The update couldn't be made runnable: " + ec.message();
        return false;
    }
    fs::rename(partial, staged, ec);
    if (ec) {
        error = "The update couldn't be saved: " + ec.message();
        return false;
    }
    return true;
}

bool RestartLauncher(std::string& error)
{
    const fs::path exe = RunningLauncherPath();
    std::vector<std::string> args = launchArguments();
    args.insert(args.begin(), exe.string());

    std::vector<char*> argv;
    for (std::string& arg : args) {
        argv.push_back(arg.data());
    }
    argv.push_back(nullptr);

    pid_t pid = 0;
    if (posix_spawn(&pid, exe.c_str(), nullptr, nullptr, argv.data(), environ) != 0) {
        error = "GWToolbox was updated, but it couldn't be restarted. Start it again from its folder.";
        return false;
    }
    return true;
}
