#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include "ChoiceWindow.h"
#include "DllDownload.h"
#include "WebViewWindow.h"

enum class InstallKind {
    Dll,
    Launcher,
};

class InstallWindow : public WebViewWindow {
public:
    InstallWindow(InstallKind kind, std::string preselected, std::function<void()> installed, std::function<void()> closed);
    ~InstallWindow() override;

    bool updated() const { return updated_; }

protected:
    std::string html() const override;
    void onClosed() override;
    void onPageReady() override;

private:
    void fetchRelease();
    void install(const std::string& directory, const Reply& reply);
    void confirmReplace(const std::string& directory, const Reply& reply);
    void startDownload(const std::string& directory);
    void chooseFolder(const Reply& reply);
    void finishDownload(bool ok, const std::string& failure);

    InstallKind kind_;
    std::string preselected_;
    std::function<void()> installed_;
    std::function<void()> closed_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    std::optional<DllRelease> release_;
    std::unique_ptr<ChoiceWindow> choice_;
    std::thread fetcher_;
    std::thread installer_;
    bool updated_ = false;
};
