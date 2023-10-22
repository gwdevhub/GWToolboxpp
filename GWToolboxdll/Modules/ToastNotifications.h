#pragma once

#include <ToolboxModule.h>

#include <wintoast/wintoastlib.h>

class ToastNotifications : public ToolboxModule {
    ToastNotifications() = default;
    ~ToastNotifications() override = default;

public:
    static ToastNotifications& Instance()
    {
        static ToastNotifications instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Notifications"; }
    [[nodiscard]] const char* Description() const override { return "Enables desktop notifications when in-game events happen."; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_BULLHORN; }

    void Initialize() override;
    void Terminate() override;
    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni*) override;
    void SaveSettings(ToolboxIni*) override;

    class Toast;
    // Runs when a toast is activated, deactivated or destructed. Will only trigger once.
    using OnToastCallback = void(__cdecl*)(const Toast* toast, bool is_activated);

    class Toast : public WinToastLib::IWinToastHandler {
    public:
        std::wstring title;
        std::wstring message;
        OnToastCallback callback = nullptr;
        void* extra_args = nullptr;
        WinToastLib::WinToastTemplate* toast_template = nullptr;
        Toast(const std::wstring& _title, const std::wstring& _message);
        ~Toast() override;
        // Public interfaces
        void toastActivated() const override;
        void toastActivated(int) const override;
        void toastDismissed(WinToastDismissalReason) const override;
        void toastFailed() const override;
        INT64 toast_id = -1;
        bool send();
        bool dismiss();
    };


    // Check whether the current OS version/platform supports Toasts
    static bool IsCompatible();
    // Show windows toast message.
    static Toast* SendToast(const wchar_t* title, const wchar_t* message, OnToastCallback callback = nullptr, void* callback_args = nullptr);
    // Dismiss a toast matching a title
    static bool DismissToast(const wchar_t* title);
};
