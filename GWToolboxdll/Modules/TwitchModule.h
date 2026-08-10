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

    [[nodiscard]] const char* Name() const override { return "Twitch集成"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_HEADSET; }
    [[nodiscard]] const char* Description() const override { return " - 直接在聊天中显示正在进行的 X 直播中的实时聊天内容。\n  - 允许向“X”发送私信，以便从激战向 X 发送消息"; }
    [[nodiscard]] const char* SettingsName() const override { return "第三方集成"; }

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
