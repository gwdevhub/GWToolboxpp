#include <cstdio>
#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

#include "DllDownload.h"
#include "DllVersion.h"
#include "FileDialog.h"
#include "Inject.h"
#include "Installation.h"
#include "InstallWindow.h"
#include "Launcher.h"
#include "LauncherSettings.h"
#include "LauncherUpdate.h"
#include "Page.h"
#include "ScanResult.h"
#include "SettingsWindow.h"
#include "WebViewWindow.h"

namespace fs = std::filesystem;

namespace {

constexpr auto kDllCheckCacheAge = std::chrono::seconds(5 * 60);

bool FetchDllReleaseCached(DllRelease& release, std::string& error)
{
    if (auto cached = CachedDllRelease(kDllCheckCacheAge)) {
        release = std::move(*cached);
        return true;
    }
    if (!FetchDllRelease(release, error)) {
        return false;
    }
    StoreDllRelease(release);
    return true;
}

std::string characterLabel(const ScanTarget& target)
{
    std::string label = target.character;
    if (!target.prefix.empty()) {
        const fs::path prefix(target.prefix);
        label += " [" + (prefix.parent_path().filename() / prefix.filename()).string() + "]";
    }
    if (target.injected) {
        label += " (injected)";
    }
    return label;
}

WindowOptions launchWindowOptions()
{
    WindowOptions options;
    options.cooldown = std::chrono::milliseconds::zero();
    options.title = "GWToolbox - Launch";
    options.width = 340;
    options.height = 320;
    options.resizable = true;
    options.autosize = true;
    options.transparent = false;
    options.centered = true;
    return options;
}

void joinWorker(std::thread& worker)
{
    if (worker.joinable()) {
        worker.join();
    }
}

class LaunchWindow : public WebViewWindow {
public:
    explicit LaunchWindow(std::string startup_error) : WebViewWindow(launchWindowOptions())
    {
        launch_error_ = std::move(startup_error);
        handler().on("getState", [this](WebViewWindow&, const Json&, const Reply& reply) {
            reply.result(state());
        });
        handler().on("retry", [this](WebViewWindow&, const Json&, const Reply& reply) {
            rescan();
            reply.result(nullptr);
        });
        handler().on("launch", [this](WebViewWindow&, const Json& args, const Reply& reply) {
            launch(args.at(0).get<size_t>(), reply);
        });
        handler().on("browseDll", [this](WebViewWindow&, const Json&, const Reply& reply) {
            browseDll(reply);
        });
        handler().on("openInstall", [this](WebViewWindow&, const Json& args, const Reply& reply) {
            const std::string folder = args.is_array() && !args.empty() ? args.at(0).get<std::string>() : std::string();
            openInstall(folder);
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
        handler().on("settings", [this](WebViewWindow&, const Json&, const Reply& reply) {
            openSettings();
            reply.result(nullptr);
        });
        handler().on("updateLauncher", [this](WebViewWindow&, const Json&, const Reply& reply) {
            openLauncherUpdate();
            reply.result(nullptr);
        });
        for (const char* method : {"troubleshooting", "restartAsAdmin", "update"}) {
            handler().on(method, [](WebViewWindow&, const Json&, const Reply& reply) {
                reply.result(nullptr);
            });
        }
        refreshInstalled();
        startLauncherUpdateCheck();
        rescan();
    }

    ~LaunchWindow() override
    {
        *alive_ = false;
        installWindow_.reset();
        updateWindow_.reset();
        settingsWindow_.reset();
        joinWorker(updater_);
        joinWorker(scanner_);
        joinWorker(launcher_);
        joinWorker(launcherUpdater_);
    }

protected:
    std::string html() const override
    {
        return pageForView("launch");
    }

    void onClosed() override
    {
        stopEventLoop();
    }

private:
    void refreshInstalled()
    {
        const fs::path dll = FindInstalledDll();
        {
            std::lock_guard lock(mutex_);
            installed_ = !dll.empty();
            dll_path_ = dll.string();
            dll_version_ = dll.empty() ? std::string() : PeFileVersion(dll);
        }
        startUpdateCheck();
    }

    void startUpdateCheck()
    {
        {
            std::lock_guard lock(mutex_);
            if (!installed_ || LauncherSettingEnabled("noUpdate")) {
                update_status_.clear();
                latest_version_.clear();
                return;
            }
            update_status_ = "checking";
        }
        if (updater_.joinable()) {
            updater_.join();
        }
        updater_ = std::thread([this, alive = alive_] {
            DllRelease release;
            std::string failure;
            const bool ok = FetchDllReleaseCached(release, failure);
            runOnUiThread([this, alive, ok, release] {
                if (*alive) {
                    finishUpdateCheck(ok, release.version);
                }
            });
        });
    }

    void finishUpdateCheck(const bool ok, const std::string& latest)
    {
        std::lock_guard lock(mutex_);
        if (!ok || latest.empty() || dll_version_.empty()) {
            update_status_ = "unknown";
            latest_version_.clear();
            return;
        }
        latest_version_ = latest;
        update_status_ = dll_version_.starts_with(latest) ? "current" : "outdated";
    }

    Json state() const
    {
        std::lock_guard lock(mutex_);
        Json characters = Json::array();
        for (const ScanTarget& target : scan_.targets) {
            characters.push_back(characterLabel(target));
        }

        std::string error;
        if (!launch_error_.empty()) {
            error = launch_error_;
        }
        else if (launching_) {
            error = "Injecting...";
        }
        else if (!scanning_ && scanned_ && scan_.targets.empty()) {
            error = scan_.errors.empty() ? "Guild Wars isn't running. Start the game, then click Re-scan." : scan_.errors.front().message;
        }

        return Json{
            {"installed", installed_},
            {"dllPath", dll_path_},
            {"dllVersion", dll_version_},
            {"updateStatus", update_status_},
            {"latestVersion", latest_version_},
            {"launcherUpdateAvailable", launcher_update_available_},
            {"scanning", scanning_},
            {"found", !scan_.targets.empty()},
            {"error", error},
            {"characters", characters},
            {"selected", 0},
        };
    }

    void rescan()
    {
        {
            std::lock_guard lock(mutex_);
            if (scanning_) {
                return;
            }
            scanning_ = true;
        }
        if (scanner_.joinable()) {
            scanner_.join();
        }
        scanner_ = std::thread([this] {
            ScanResult result = ScanGuildWars();
            std::lock_guard lock(mutex_);
            scan_ = std::move(result);
            scanned_ = true;
            scanning_ = false;
        });
    }

    void launch(const size_t index, const Reply& reply)
    {
        ScanTarget target;
        {
            std::lock_guard lock(mutex_);
            if (!installed_) {
                throw std::runtime_error("GWToolbox isn't installed yet.");
            }
            if (index >= scan_.targets.size()) {
                throw std::out_of_range("There's no character to launch.");
            }
            if (launching_) {
                reply.result(nullptr);
                return;
            }
            target = scan_.targets[index];
            launch_error_.clear();
            launching_ = true;
        }
        if (launcher_.joinable()) {
            launcher_.join();
        }
        launcher_ = std::thread([this, target, reply, alive = alive_] {
            const InjectOutcome outcome = injectTarget(target);
            runOnUiThread([this, reply, outcome, alive] {
                if (*alive) {
                    finishLaunch(reply, outcome);
                }
            });
        });
    }

    void finishLaunch(const Reply& reply, const InjectOutcome& outcome)
    {
        {
            std::lock_guard lock(mutex_);
            launching_ = false;
            launch_error_ = outcome.ok ? std::string() : outcome.message;
        }
        reply.result(nullptr);
        if (outcome.ok) {
            close();
        }
    }

    void startLauncherUpdateCheck()
    {
        if (LauncherSettingEnabled("noUpdate")) {
            return;
        }
        launcherUpdater_ = std::thread([this, alive = alive_] {
            DllRelease release;
            std::string failure;
            const bool available = FetchLauncherRelease(release, failure);
            runOnUiThread([this, alive, available] {
                if (*alive) {
                    std::lock_guard lock(mutex_);
                    launcher_update_available_ = available;
                }
            });
        });
    }

    void openLauncherUpdate()
    {
        if (updateWindow_) {
            updateWindow_->show();
            return;
        }
        updateWindow_ = std::make_unique<InstallWindow>(
            InstallKind::Launcher,
            RunningLauncherPath().parent_path().string(),
            nullptr,
            [this, alive = alive_] {
                runOnUiThread([this, alive] {
                    if (*alive) {
                        finishLauncherUpdate();
                    }
                });
            });
        updateWindow_->show();
    }

    void finishLauncherUpdate()
    {
        const bool updated = updateWindow_->updated();
        updateWindow_.reset();
        if (!updated) {
            return;
        }
        std::string error;
        if (RestartLauncher(error)) {
            close();
            return;
        }
        std::lock_guard lock(mutex_);
        launch_error_ = error;
    }

    void browseDll(const Reply& reply)
    {
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
        refreshInstalled();
        reply.result(Json{{"ok", true}, {"message", std::string()}});
    }

    void openSettings()
    {
        if (settingsWindow_) {
            settingsWindow_->show();
            return;
        }
        settingsWindow_ = std::make_unique<SettingsWindow>(
            [this, alive = alive_] {
                if (*alive) {
                    refreshInstalled();
                }
            },
            [this, alive = alive_] {
                runOnUiThread([this, alive] {
                    if (*alive) {
                        settingsWindow_.reset();
                    }
                });
            });
        settingsWindow_->show();
    }

    void openInstall(const std::string& folder)
    {
        if (installWindow_) {
            installWindow_->show();
            return;
        }
        installWindow_ = std::make_unique<InstallWindow>(
            InstallKind::Dll,
            folder,
            [this, alive = alive_] {
                if (*alive) {
                    refreshInstalled();
                }
            },
            [this, alive = alive_] {
                runOnUiThread([this, alive] {
                    if (*alive) {
                        installWindow_.reset();
                    }
                });
            });
        installWindow_->show();
    }

    mutable std::mutex mutex_;
    ScanResult scan_;
    bool scanned_ = false;
    bool scanning_ = false;
    bool launching_ = false;
    bool installed_ = false;
    std::string dll_path_;
    std::string dll_version_;
    std::string update_status_;
    std::string latest_version_;
    std::string launch_error_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    bool launcher_update_available_ = false;
    std::unique_ptr<InstallWindow> installWindow_;
    std::unique_ptr<InstallWindow> updateWindow_;
    std::unique_ptr<SettingsWindow> settingsWindow_;
    std::thread scanner_;
    std::thread launcher_;
    std::thread updater_;
    std::thread launcherUpdater_;
};

}

int RunLauncher(std::string startup_error)
{
    setApplicationName("GWToolbox");
    RemoveLauncherLeftovers(RunningLauncherPath());
    try {
        initializeWindowing();
    }
    catch (const RuntimeMissingError& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }

    LaunchWindow window(std::move(startup_error));
    window.show();
    runEventLoop();
    return 0;
}

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
