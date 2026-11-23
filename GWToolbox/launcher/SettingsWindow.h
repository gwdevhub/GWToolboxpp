#pragma once

#include <functional>
#include <string>

#include "WebViewWindow.h"

class SettingsWindow : public WebViewWindow {
public:
    SettingsWindow(std::function<void()> changed, std::function<void()> closed);

protected:
    std::string html() const override;
    void onClosed() override;

private:
    std::function<void()> changed_;
    std::function<void()> closed_;
};
