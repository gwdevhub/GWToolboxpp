#include "SettingsWindow.h"

#include <filesystem>
#include <optional>
#include <stdexcept>

#include "DllDownload.h"
#include "DllVersion.h"
#include "Elevation.h"
#include "FileDialog.h"
#include "Installation.h"
#include "LauncherSettings.h"
#include "Page.h"

namespace fs = std::filesystem;

namespace {

WindowOptions settingsWindowOptions()
{
    WindowOptions options;
    options.cooldown = std::chrono::milliseconds::zero();
    options.title = "GWToolbox - Settings";
    options.width = 305;
    options.height = 135;
    options.resizable = true;
    options.autosize = true;
    options.transparent = false;
    options.centered = true;
    return options;
}

}

SettingsWindow::SettingsWindow(std::function<void()> changed, std::function<void()> closed)
    : WebViewWindow(settingsWindowOptions())
    , changed_(std::move(changed))
    , closed_(std::move(closed))
{
    handler().on("getState", [](WebViewWindow&, const Json&, const Reply& reply) {
        const fs::path dll = FindInstalledDll();
        reply.result(Json{{"noUpdate", LauncherSettingEnabled("noUpdate")},
                          {"dllPath", dll.string()},
                          {"dllVersion", dll.empty() ? std::string() : PeFileVersion(dll)},
                          {"canRestartAsAdmin", CanRestartLauncherAsAdmin()}});
    });
    handler().on("restartAsAdmin", [](WebViewWindow&, const Json&, const Reply& reply) {
        std::string error;
        if (!RestartLauncherAsAdmin(error)) {
            reply.result(Json{{"ok", false}, {"message", error}});
            return;
        }
        reply.result(Json{{"ok", true}, {"message", std::string()}});
        stopEventLoop();
    });
    handler().on("browseDll", [this](WebViewWindow&, const Json&, const Reply& reply) {
        const std::optional<std::string> picked = ChooseDllFile(SuggestedInstallDirectory().string());
        if (!picked) {
            reply.result(Json{{"ok", false}, {"message", std::string()}});
            return;
        }
        std::string error;
        if (!UseDll(*picked, error)) {
            reply.result(Json{{"ok", false}, {"message", error}});
            return;
        }
        if (changed_) {
            changed_();
        }
        reply.result(Json{{"ok", true}, {"message", std::string()}});
    });
    handler().on("setOption", [this](WebViewWindow&, const Json& args, const Reply& reply) {
        const auto name = args.at(0).get<std::string>();
        const auto value = args.at(1).get<bool>();
        if (name != "noUpdate") {
            throw std::invalid_argument("Unknown setting: " + name);
        }
        SetLauncherSetting(name, value);
        if (changed_) {
            changed_();
        }
        reply.result(nullptr);
    });
    handler().on("fitToPage", [](WebViewWindow& window, const Json& args, const Reply& reply) {
        window.fitToPage(args.at(0).get<int>(), args.at(1).get<int>());
        reply.result(nullptr);
    });
    handler().on("close", [](WebViewWindow& window, const Json&, const Reply& reply) {
        window.close();
        reply.result(nullptr);
    });
    handler().on("beginMove", [](WebViewWindow& window, const Json&, const Reply& reply) {
        window.beginMove();
        reply.result(nullptr);
    });
}

std::string SettingsWindow::html() const
{
    return pageForView("settings");
}

void SettingsWindow::onClosed()
{
    if (closed_) {
        closed_();
    }
}
