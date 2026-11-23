#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <spawn.h>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <vector>

#include "Command.h"
#include "DllDownload.h"
#include "LauncherPaths.h"
#include "LauncherUpdate.h"
#include "Messages.h"

namespace fs = std::filesystem;

extern char** environ;

namespace {

constexpr auto kReleasesUrl = "https://api.github.com/repos/gwdevhub/GWToolboxpp/releases?per_page=30";
constexpr auto kCheckTimeout = std::chrono::seconds(60);
constexpr auto kDownloadTimeout = std::chrono::seconds(600);
constexpr auto kPollInterval = std::chrono::milliseconds(200);
constexpr const char* kLauncherAsset = "GWToolboxLinux";
constexpr const char* kAppImageSuffix = ".AppImage";

bool isLauncherAsset(const std::string& name)
{
    if (runningFromAppImage()) {
        return name.starts_with("GWToolbox") && name.ends_with(kAppImageSuffix);
    }
    return name == kLauncherAsset;
}

std::string lowerCase(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::string sha256Of(const fs::path& file)
{
    const CommandOutput output = runCommand({"sha256sum", file.string()}, currentEnvironment(), kCheckTimeout);
    if (output.exit_code != 0) {
        return {};
    }
    return lowerCase(output.text.substr(0, output.text.find_first_of(" \n")));
}

std::string digestHex(const std::string& digest)
{
    constexpr std::string_view prefix = "sha256:";
    if (!digest.starts_with(prefix)) {
        return {};
    }
    return lowerCase(digest.substr(prefix.size()));
}

fs::path stagedPath(const fs::path& current)
{
    return current.parent_path() / (".new" + current.extension().string());
}

fs::path oldPath(const fs::path& current)
{
    return fs::path(current.string() + ".old");
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
    const Json releases = Json::parse(listing.text, nullptr, false);
    if (!releases.is_array()) {
        error = "The GWToolbox release list couldn't be read.";
        return false;
    }

    for (const Json& candidate : releases) {
        if (candidate.value("draft", false) || !candidate.contains("assets")) {
            continue;
        }
        for (const Json& asset : candidate["assets"]) {
            if (!isLauncherAsset(asset.value("name", std::string()))) {
                continue;
            }
            const std::string sha256 = digestHex(asset.value("digest", std::string()));
            if (!sha256.empty() && sha256Of(RunningLauncherPath()) == sha256) {
                error.clear();
                return false;
            }
            release.version = candidate.value("tag_name", std::string());
            release.changelog = "GWToolbox " + release.version + "\n\n" + candidate.value("body", std::string());
            release.url = asset.value("browser_download_url", std::string());
            release.size = asset.value("size", std::uintmax_t(0));
            release.sha256 = sha256;
            return !release.url.empty();
        }
    }
    error = runningFromAppImage() ? "No GWToolbox release has an AppImage to update to yet." : std::string();
    return false;
}

bool DownloadLauncherRelease(const DllRelease& release, const fs::path& staged, const std::function<void(int)>& progress, std::string& error)
{
    if (release.sha256.empty()) {
        error = "GitHub did not provide a checksum for the update.";
        return false;
    }

    const fs::path partial = fs::path(staged.string() + ".part");
    std::atomic<bool> finished{false};
    std::thread poller([&] {
        int last = -1;
        while (!finished) {
            std::error_code ec;
            const std::uintmax_t size = fs::file_size(partial, ec);
            if (!ec && release.size > 0) {
                const int percent = static_cast<int>(std::min<std::uintmax_t>(100, size * 100 / release.size));
                if (percent != last) {
                    last = percent;
                    progress(percent);
                }
            }
            std::this_thread::sleep_for(kPollInterval);
        }
    });

    const CommandOutput download = runCommand({"curl", "-fsSL", "-o", partial.string(), release.url}, currentEnvironment(), kDownloadTimeout);
    finished = true;
    poller.join();

    std::error_code ec;
    if (download.exit_code != 0) {
        fs::remove(partial, ec);
        error = "The update couldn't be downloaded (curl exit " + std::to_string(download.exit_code) + ").";
        return false;
    }
    if (sha256Of(partial) != release.sha256) {
        fs::remove(partial, ec);
        error = "The update's checksum didn't match the release.";
        return false;
    }
    fs::rename(partial, staged, ec);
    if (ec) {
        error = "The update couldn't be saved: " + ec.message();
        return false;
    }
    return true;
}

bool InstallLauncherRelease(const fs::path& current, const fs::path& staged, std::string& error)
{
    std::error_code ec;
    fs::permissions(staged,
                    fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec | fs::perms::others_read | fs::perms::others_exec,
                    fs::perm_options::replace, ec);
    if (ec) {
        fs::remove(staged, ec);
        error = "The update couldn't be made runnable: " + ec.message();
        return false;
    }

    const fs::path old = oldPath(current);
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

void RemoveLauncherLeftovers(const fs::path& current)
{
    std::error_code ec;
    fs::remove(oldPath(current), ec);
    fs::remove(stagedPath(current), ec);
    fs::remove(fs::path(stagedPath(current).string() + ".part"), ec);
}
