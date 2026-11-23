#include "InstallWindow.h"

#include <filesystem>
#include <optional>
#include <string>

#include "DllDownload.h"
#include "DllVersion.h"
#include "FileDialog.h"
#include "LauncherUpdate.h"
#include "Page.h"

namespace fs = std::filesystem;

namespace {

WindowOptions installWindowOptions(const InstallKind kind)
{
    WindowOptions options;
    options.cooldown = std::chrono::milliseconds::zero();
    options.title = kind == InstallKind::Dll ? "GWToolbox - Install" : "GWToolbox - Update";
    options.width = 800;
    options.height = 600;
    options.resizable = true;
    options.autosize = false;
    options.transparent = false;
    options.centered = true;
    return options;
}

bool DownloadAndInstallLauncher(const DllRelease& release, const std::function<void(int)>& progress, std::string& error)
{
    const fs::path current = RunningLauncherPath();
    const fs::path staged = current.parent_path() / (".new" + current.extension().string());
    return DownloadLauncherRelease(release, staged, progress, error) && InstallLauncherRelease(current, staged, error);
}

}

InstallWindow::InstallWindow(const InstallKind kind, std::string preselected, std::function<void()> installed, std::function<void()> closed)
    : WebViewWindow(installWindowOptions(kind))
    , kind_(kind)
    , preselected_(std::move(preselected))
    , installed_(std::move(installed))
    , closed_(std::move(closed))
{
    handler().on("getState", [this](WebViewWindow&, const Json&, const Reply& reply) {
        const std::string suggested = preselected_.empty() ? SuggestedInstallDirectory().string() : preselected_;
        reply.result(Json{{"suggested", suggested}});
    });
    handler().on("chooseFolder", [this](WebViewWindow&, const Json&, const Reply& reply) {
        chooseFolder(reply);
    });
    handler().on("install", [this](WebViewWindow&, const Json& args, const Reply& reply) {
        install(args.at(0).get<std::string>(), reply);
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

InstallWindow::~InstallWindow()
{
    *alive_ = false;
    if (fetcher_.joinable()) {
        fetcher_.join();
    }
    if (installer_.joinable()) {
        installer_.join();
    }
}

void InstallWindow::onPageReady()
{
    fetchRelease();
}

void InstallWindow::fetchRelease()
{
    if (fetcher_.joinable()) {
        fetcher_.join();
    }
    fetcher_ = std::thread([this, alive = alive_] {
        DllRelease release;
        std::string failure;
        const bool ok = kind_ == InstallKind::Dll ? FetchDllRelease(release, failure) : FetchLauncherRelease(release, failure);
        runOnUiThread([this, alive, ok, release, failure] {
            if (!*alive) {
                return;
            }
            if (ok) {
                release_ = release;
                emit("install", Json{{"phase", "release"}, {"changelog", release.changelog}});
            }
            else {
                emit("install", Json{{"phase", "release"}, {"error", failure.empty() ? std::string("GWToolbox is up to date.") : failure}});
            }
        });
    });
}

void InstallWindow::onClosed()
{
    if (closed_) {
        closed_();
    }
}

std::string InstallWindow::html() const
{
    return pageForView(kind_ == InstallKind::Dll ? "install" : "update");
}

void InstallWindow::chooseFolder(const Reply& reply)
{
    const std::string start = preselected_.empty() ? SuggestedInstallDirectory().string() : preselected_;
    const std::string picked = ChooseFolder(start).value_or(std::string());
    reply.result(Json{{"path", picked}});
}

void InstallWindow::install(const std::string& directory, const Reply& reply)
{
    if (kind_ == InstallKind::Launcher) {
        reply.result(Json{{"ok", true}, {"message", std::string()}});
        startDownload(directory);
        return;
    }
    std::string error;
    if (!PrepareInstallDirectory(directory, error)) {
        reply.result(Json{{"ok", false}, {"message", error}});
        return;
    }
    if (fs::exists(fs::path(directory) / "GWToolboxdll.dll")) {
        confirmReplace(directory, reply);
        return;
    }
    reply.result(Json{{"ok", true}, {"message", std::string()}});
    startDownload(directory);
}

void InstallWindow::confirmReplace(const std::string& directory, const Reply& reply)
{
    const std::string current = PeFileVersion(fs::path(directory) / "GWToolboxdll.dll");
    const std::string existing = current.empty() ? std::string() : " (version " + current + ")";
    const std::string message = "A GWToolboxdll.dll" + existing + " is already in\n" + directory + "\n\nReplace it with the latest release?";
    choice_ = std::make_unique<ChoiceWindow>("Replace GWToolboxdll.dll?", message, std::vector<std::string>{"Replace", "Cancel"},
        [this, alive = alive_, directory, reply](std::optional<std::size_t> answer) {
            if (!*alive) {
                return;
            }
            if (answer && *answer == 0) {
                reply.result(Json{{"ok", true}, {"message", std::string()}});
                startDownload(directory);
            }
            else {
                reply.result(Json{{"ok", false}, {"message", std::string()}});
            }
            runOnUiThread([this, alive] {
                if (*alive) {
                    choice_.reset();
                }
            });
        });
    choice_->show();
}

void InstallWindow::startDownload(const std::string& directory)
{
    if (installer_.joinable()) {
        installer_.join();
    }
    const std::optional<DllRelease> known = release_;
    installer_ = std::thread([this, directory, known, alive = alive_] {
        DllRelease release;
        std::string failure;
        bool ok = known.has_value();
        if (known) {
            release = *known;
        }
        else {
            ok = kind_ == InstallKind::Dll ? FetchDllRelease(release, failure) : FetchLauncherRelease(release, failure);
        }
        if (ok) {
            const auto progress = [this, alive](int percent) {
                runOnUiThread([this, alive, percent] {
                    if (*alive) {
                        emit("install", Json{{"phase", "progress"}, {"percent", percent}});
                    }
                });
            };
            ok = kind_ == InstallKind::Dll ? DownloadDllFile(release, directory, progress, failure)
                                           : DownloadAndInstallLauncher(release, progress, failure);
        }
        runOnUiThread([this, alive, ok, failure] {
            if (!*alive) {
                return;
            }
            finishDownload(ok, failure);
        });
    });
}

void InstallWindow::finishDownload(const bool ok, const std::string& failure)
{
    updated_ = ok && kind_ == InstallKind::Launcher;
    const std::string message = ok ? std::string(kind_ == InstallKind::Dll ? "GWToolboxdll.dll is installed." : "GWToolbox is updated.") : failure;
    emit("install", Json{{"phase", "done"}, {"ok", ok}, {"message", message}});
    if (ok && installed_) {
        installed_();
    }
}
