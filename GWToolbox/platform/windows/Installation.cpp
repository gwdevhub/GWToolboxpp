#include "stdafx.h"

#include "Installation.h"

#include <fstream>
#include <optional>

#include <Path.h>
#include "Install.h"
#include "LauncherSettings.h"
#include "Settings.h"
#include "Utf8.h"
#include "WebViewWindow.h"

namespace fs = std::filesystem;

namespace {

fs::path settingsFile()
{
    fs::path directory;
    PathGetProgramDirectory(directory);
    return directory / L"GWToolbox.json";
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

void writeSettings(const Json& settings)
{
    std::ofstream(settingsFile()) << settings.dump(2) << '\n';
}

std::optional<fs::path> dllFromSettings()
{
    const Json settings = readSettings();
    if (!settings.contains("dll") || !settings["dll"].is_string()) {
        return std::nullopt;
    }
    const fs::path dll = FromUtf8(settings["dll"].get<std::string>());
    std::error_code ec;
    if (fs::is_regular_file(dll, ec)) {
        return dll;
    }
    return std::nullopt;
}

}

fs::path FindInstalledDll()
{
    if (const auto dll = dllFromSettings()) {
        return *dll;
    }
    const fs::path local = GetInstallationDir().parent_path() / L"GWToolboxdll.dll";
    std::error_code ec;
    if (fs::is_regular_file(local, ec)) {
        RecordInstalledDll(local);
        return local;
    }
    return {};
}

void RecordInstalledDll(const fs::path& dll)
{
    Json settings = readSettings();
    settings["dll"] = ToUtf8(dll.wstring());
    writeSettings(settings);
}

bool LauncherSettingEnabled(const std::string& name)
{
    if (name == "noUpdate" && settings.noupdate) {
        return true;
    }
    const Json saved = readSettings();
    return saved.contains(name) && saved[name].is_boolean() && saved[name].get<bool>();
}

void SetLauncherSetting(const std::string& name, const bool value)
{
    Json saved = readSettings();
    saved[name] = value;
    writeSettings(saved);
    if (name == "noUpdate") {
        settings.noupdate = value;
    }
}
