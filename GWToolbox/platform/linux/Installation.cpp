#include "Installation.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <string>
#include <system_error>
#include <thread>

#include "Command.h"
#include "DllDownload.h"
#include "LauncherPaths.h"
#include "LauncherSettings.h"
#include "Messages.h"

namespace fs = std::filesystem;

namespace {

bool localDllOnly = false;

constexpr auto kCheckTimeout = std::chrono::seconds(60);
constexpr auto kDownloadTimeout = std::chrono::seconds(600);
constexpr auto kPollInterval = std::chrono::milliseconds(200);
constexpr auto kReleasesUrl = "https://api.github.com/repos/gwdevhub/GWToolboxpp/releases?per_page=30";

fs::path settingsFile()
{
    return launcherDirectory() / "GWToolbox.json";
}

Json readSettings()
{
    std::ifstream in(settingsFile());
    if (!in) {
        return Json::object();
    }
    Json settings = Json::parse(in, nullptr, false);
    return settings.is_object() ? settings : Json::object();
}

std::optional<fs::path> dllFromSettings()
{
    const Json settings = readSettings();
    if (!settings.contains("dll") || !settings["dll"].is_string()) {
        return std::nullopt;
    }
    const fs::path dll = settings["dll"].get<std::string>();
    std::error_code ec;
    if (fs::is_regular_file(dll, ec)) {
        return dll;
    }
    return std::nullopt;
}

std::optional<Json> dllAsset(const Json& release)
{
    if (!release.contains("assets")) {
        return std::nullopt;
    }
    for (const Json& asset : release["assets"]) {
        const std::string name = asset.value("name", std::string());
        if (name == "GWToolboxdll.dll" || name == "GWToolbox.dll") {
            return asset;
        }
    }
    return std::nullopt;
}

}

fs::path FindInstalledDll()
{
    const fs::path local = launcherDirectory() / "GWToolboxdll.dll";
    std::error_code ec;
    if (localDllOnly) {
        return fs::is_regular_file(local, ec) ? local : fs::path{};
    }
    if (const auto dll = dllFromSettings()) {
        return *dll;
    }
    if (fs::is_regular_file(local, ec)) {
        RecordInstalledDll(local);
        return local;
    }
    return {};
}

void SetLocalDllOnly(const bool enabled)
{
    localDllOnly = enabled;
}

void RecordInstalledDll(const fs::path& dll)
{
    Json settings = readSettings();
    settings["dll"] = dll.string();
    std::ofstream(settingsFile()) << settings.dump(2) << '\n';
}

bool FetchDllRelease(DllRelease& release, std::string& error)
{
    const auto listing = runCommand({"curl", "-fsSL", "-H", "Accept: application/vnd.github+json", "-H", "User-Agent: GWToolbox", kReleasesUrl},
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
        const auto asset = dllAsset(candidate);
        if (candidate.value("draft", false) || !asset) {
            continue;
        }
        release.url = asset->value("browser_download_url", std::string());
        release.size = asset->value("size", std::uintmax_t(0));
        release.changelog = "GWToolbox " + candidate.value("tag_name", std::string()) + "\n\n" + candidate.value("body", std::string());
        release.version = std::regex_replace(candidate.value("tag_name", std::string()), std::regex(R"([^0-9.])"), "");
        if (release.url.empty()) {
            error = "The GWToolboxdll.dll release has no download address.";
            return false;
        }
        return true;
    }
    error = "No GWToolboxdll.dll was found in the GWToolbox releases.";
    return false;
}

bool DownloadDllFile(const DllRelease& release, const fs::path& directory, const std::function<void(int)>& progress, std::string& error)
{
    const fs::path target = directory / "GWToolboxdll.dll";
    const fs::path partial = fs::path(target.string() + ".part");

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

    const auto download = runCommand({"curl", "-fsSL", "-o", partial.string(), release.url}, currentEnvironment(), kDownloadTimeout);
    finished = true;
    poller.join();

    std::error_code ec;
    if (download.exit_code != 0) {
        fs::remove(partial, ec);
        error = "GWToolboxdll.dll couldn't be downloaded (curl exit " + std::to_string(download.exit_code) + ").";
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

fs::path SuggestedInstallDirectory()
{
    return launcherDirectory();
}

bool PrepareInstallDirectory(const fs::path& directory, std::string& error)
{
    std::error_code ec;
    fs::create_directories(directory, ec);
    if (ec || !fs::is_directory(directory, ec)) {
        error = "The folder " + directory.string() + " couldn't be created.";
        return false;
    }
    const fs::path probe = directory / ".gwtoolbox-write-test";
    std::ofstream(probe) << "";
    const bool writable = fs::exists(probe, ec);
    fs::remove(probe, ec);
    if (!writable) {
        error = "The folder " + directory.string() + " isn't writable.";
        return false;
    }
    return true;
}

bool UseDll(const fs::path& dll, std::string& error)
{
    std::error_code ec;
    if (dll.extension() != ".dll" || !fs::is_regular_file(dll, ec)) {
        error = "Choose a GWToolboxdll.dll file.";
        return false;
    }
    RecordInstalledDll(dll);
    return true;
}

bool LauncherSettingEnabled(const std::string& name)
{
    const Json settings = readSettings();
    return settings.contains(name) && settings[name].is_boolean() && settings[name].get<bool>();
}

void SetLauncherSetting(const std::string& name, bool value)
{
    Json settings = readSettings();
    settings[name] = value;
    std::ofstream(settingsFile()) << settings.dump(2) << '\n';
}
