#pragma once

#include "IRC.h"
#include <ToolboxModule.h>

class TwitchModule : public ToolboxModule {
    TwitchModule() = default;
    TwitchModule(const TwitchModule&) = delete;

    ~TwitchModule() override = default;

public:
    static TwitchModule& Instance()
    {
        static TwitchModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Twitch"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_HEADSET; }
    [[nodiscard]] const char* Description() const override { return " - Show the live chat from a running Twitch stream directly in chat.\n - Send a whisper to 'Twitch' to send a message to Twitch from GW"; }
    [[nodiscard]] const char* SettingsName() const override { return "Third Party Integration"; }

    struct Settings {
        std::string irc_alias = "Twitch";
        std::string irc_server = "irc.chat.twitch.tv";
        std::string irc_username;
        std::string irc_password = "oauth:<your_token_here>";
        std::string irc_channel;
        bool twitch_enabled = true;
        bool show_messages = true;
        bool notify_on_user_join = true;
        bool notify_on_user_leave = true;
        Colors::SettingColor irc_chat_color = Colors::RGB(0xAD, 0x83, 0xFA);
    };

    void Initialize() override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void Update(float delta) override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;

    static bool Connect();
    static void Disconnect();
    static bool IsConnected();
    static IRC* irc();
};
