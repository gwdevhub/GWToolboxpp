#include "Installation.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>

#include "Command.h"
#include "DllDownload.h"
#include "Download.h"
#include "LauncherPaths.h"
#include "LauncherSettings.h"
#include "Messages.h"

namespace fs = std::filesystem;

namespace {

constexpr auto kCheckTimeout = std::chrono::seconds(60);
constexpr auto kReleasesUrl = "https://api.github.com/repos/gwdevhub/GWToolboxpp/releases?per_page=30";

}

fs::path launcherSettingsFile()
{
    return launcherDirectory() / "GWToolbox.json";
}

fs::path LocalDllPath()
{
    return launcherDirectory() / "GWToolboxdll.dll";
}

fs::path DefaultDllPath()
{
    return launcherDirectory() / "GWToolboxdll.dll";
}

bool FetchDllRelease(DllRelease& release, std::string& error)
{
    const auto listing = runCommand({"curl", "-fsSL", "-H", "Accept: application/vnd.github+json", "-H", "User-Agent: GWToolbox", kReleasesUrl},
                                    currentEnvironment(), kCheckTimeout);
    if (listing.exit_code != 0) {
        error = "The GWToolbox release list couldn't be downloaded (curl exit " + std::to_string(listing.exit_code) + ").";
        return false;
    }
    return DllReleaseFromListing(listing.text, release, error);
}

bool DownloadDllFile(const DllRelease& release, const fs::path& directory, const std::function<void(int)>& progress, std::string& error)
{
    const fs::path target = directory / "GWToolboxdll.dll";
    const fs::path partial = fs::path(target.string() + ".part");

    const int exit_code = downloadWithProgress(release.url, partial, release.size, progress);

    std::error_code ec;
    if (exit_code != 0) {
        fs::remove(partial, ec);
        error = "GWToolboxdll.dll couldn't be downloaded (curl exit " + std::to_string(exit_code) + ").";
        return false;
    }
    fs::rename(partial, target, ec);
    if (ec) {
        error = "GWToolboxdll.dll couldn't be saved: " + ec.message();
        return false;
    }
    RecordInstalledDll(target);
    return true;
}

