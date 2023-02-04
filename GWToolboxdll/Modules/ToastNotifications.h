#pragma once

#include <ToolboxModule.h>

#include <wintoast/wintoastlib.h>

class ToastNotifications : public ToolboxModule {
    ToastNotifications() = default;
    ~ToastNotifications() = default;

public:
    static ToastNotifications& Instance()
    {
        static ToastNotifications instance;
        return instance;
    }

    const char* Name() const override { return "Notifications"; }
    const char* Description() const override { return "Enables desktop notifications when in-game events happen."; }
    const char* Icon() const override { return ICON_FA_BULLHORN; }

    void Initialize() override;
    void Terminate() override;
    void DrawSettingInternal() override;
    void LoadSettings(ToolboxIni*) override;
    void SaveSettings(ToolboxIni*) override;

    class Toast;
    // Runs when a toast is activated, deactivated or destructed. Will only trigger once.
    typedef void(__cdecl* OnToastCallback)(const Toast* toast, bool is_activated);

    class Toast : public WinToastLib::IWinToastHandler {
    public:
        std::wstring title;
        std::wstring message;
        OnToastCallback callback = 0;
        void* extra_args = 0;
        WinToastLib::WinToastTemplate* toast_template = 0;
        Toast(std::wstring _title, std::wstring _message);
        ~Toast();
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
    static Toast* SendToast(const wchar_t* title, const wchar_t* message, OnToastCallback callback = 0, void* callback_args = 0);
    // Dismiss a toast matching a title
    static bool DismissToast(const wchar_t* title);
};
