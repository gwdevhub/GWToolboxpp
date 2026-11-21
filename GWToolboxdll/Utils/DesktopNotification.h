#pragma once

#include <functional>
#include <memory>
#include <string>

class DesktopNotification {
public:
    struct Handlers {
        std::function<void()> activated;
        std::function<void()> dismissed;
        std::function<void(HRESULT)> failed;
    };

    static bool IsSupported();
    static bool RegisterApp(const std::wstring& app_name, const std::wstring& app_user_model_id);
    static std::unique_ptr<DesktopNotification> Show(const std::wstring& app_user_model_id, const std::wstring& title, const std::wstring& message, Handlers handlers);

    ~DesktopNotification();

    struct Impl;

private:
    explicit DesktopNotification(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl;
};
