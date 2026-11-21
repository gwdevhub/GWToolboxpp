#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "WebViewWindow.h"

bool StartLauncherWindowing();
void PumpMessagesFor(std::uint32_t timeout_ms);
void PumpMessagesUntil(const std::function<bool()>& done);
std::string ToUtf8(std::wstring_view text);

class LauncherWindow : public WebViewWindow {
public:
    LauncherWindow(std::string title, int width, int height, int page_resource_id, std::wstring frame_key);

    bool closed() const { return closed_; }

protected:
    std::string html() const override;
    void onClosed() override { closed_ = true; }
    void onFrameChanged() override;
    void onBeforeFirstShow() override;

private:
    std::wstring frameValueName(const wchar_t* part) const;

    int page_resource_id_;
    std::wstring frame_key_;
    bool closed_ = false;
};
