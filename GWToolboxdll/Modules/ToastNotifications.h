#pragma once

#include <ToolboxModule.h>

#include <wintoastlib.h>

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

    struct Settings {
        bool show_notifications_when_focussed = false;
        bool show_notifications_when_in_background = true;
        bool show_notifications_when_minimised = true;
        bool show_notifications_when_in_outpost = true;
        bool show_notifications_when_in_explorable = true;

        bool show_notifications_on_whisper = true;
        bool show_notifications_on_guild_chat = false;
        bool show_notifications_on_ally_chat = false;
        bool show_notifications_on_team_chat = false;
        bool show_notifications_on_last_to_ready = false;
        bool show_notifications_on_invite = false;
        bool show_notifications_on_everyone_ready = false;
        bool show_notifications_on_self_resurrected = false;

        bool flash_window_on_whisper = true;
        bool flash_window_on_guild_chat = false;
        bool flash_window_on_ally_chat = false;
        bool flash_window_on_team_chat = false;
        bool flash_window_on_last_to_ready = false;
        bool flash_window_on_invite = false;
        bool flash_window_on_everyone_ready = false;
        bool flash_window_on_self_resurrected = false;

        bool change_title_on_notification = false;
    };

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Terminate() override;
    void DrawSettingsInternal() override;
    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

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
        void toastFailed(HRESULT err = 0) const override;
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
