#include "LauncherSettings.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>

#include "DllDownload.h"
#include "Installation.h"
#include "Messages.h"

namespace fs = std::filesystem;

namespace {

Json readSettings()
{
    std::ifstream in(launcherSettingsFile());
    if (!in) {
        return Json::object();
    }
    Json settings = Json::parse(in, nullptr, false);
    return settings.is_object() ? settings : Json::object();
}

void writeSettings(const Json& settings)
{
    std::ofstream(launcherSettingsFile()) << settings.dump(2) << '\n';
}

}

bool LauncherSettingEnabled(const std::string& name)
{
    const Json saved = readSettings();
    return saved.contains(name) && saved[name].is_boolean() && saved[name].get<bool>();
}

void SetLauncherSetting(const std::string& name, const bool value)
{
    Json saved = readSettings();
    saved[name] = value;
    writeSettings(saved);
}

std::optional<fs::path> dllFromSettings()
{
    const Json saved = readSettings();
    if (!saved.contains("dll") || !saved["dll"].is_string()) {
        return std::nullopt;
    }
    const std::string utf8 = saved["dll"].get<std::string>();
    const fs::path dll(std::u8string(utf8.begin(), utf8.end()));
    std::error_code ec;
    if (fs::is_regular_file(dll, ec)) {
        return dll;
    }
    return std::nullopt;
}

void RecordInstalledDll(const fs::path& dll)
{
    Json saved = readSettings();
    const std::u8string utf8 = dll.u8string();
    saved["dll"] = std::string(utf8.begin(), utf8.end());
    writeSettings(saved);
}

std::optional<DllRelease> CachedDllRelease(const std::chrono::seconds max_age)
{
    const Json saved = readSettings();
    if (!saved.contains("dllReleaseCache") || !saved["dllReleaseCache"].is_object()) {
        return std::nullopt;
    }
    const Json& entry = saved["dllReleaseCache"];
    const auto stored = std::chrono::system_clock::time_point(std::chrono::seconds(entry.value("time", std::int64_t(0))));
    const auto age = std::chrono::system_clock::now() - stored;
    if (age < std::chrono::seconds(0) || age > max_age) {
        return std::nullopt;
    }
    DllRelease release;
    release.version = entry.value("version", std::string());
    release.changelog = entry.value("changelog", std::string());
    release.url = entry.value("url", std::string());
    release.size = entry.value("size", std::uintmax_t(0));
    release.sha256 = entry.value("sha256", std::string());
    return release;
}

void StoreDllRelease(const DllRelease& release)
{
    Json saved = readSettings();
    saved["dllReleaseCache"] = {
        {"time", std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()},
        {"version", release.version},
        {"changelog", release.changelog},
        {"url", release.url},
        {"size", release.size},
        {"sha256", release.sha256},
    };
    writeSettings(saved);
}
