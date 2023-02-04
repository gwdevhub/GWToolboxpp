#pragma once

#include "IRC.h"
#include <ToolboxModule.h>

#include <GWCA/Utilities/Hook.h>

#include <Color.h>

class TwitchModule : public ToolboxModule {
    TwitchModule() {};
    TwitchModule(const TwitchModule&) = delete;
    ~TwitchModule() {};
public:
    static TwitchModule& Instance() {
        static TwitchModule instance;
        return instance;
    }

    const char* Name() const override { return "Twitch"; }
    const char* Description() const override { return " - Show the live chat from a running Twitch stream directly in chat.\n - Send a whisper to 'Twitch' to send a message to Twitch from GW"; }
    const char* SettingsName() const override { return "Third Party Integration"; }

    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;
    bool Connect();
    void Disconnect();
    // IRC details
    std::string irc_server="irc.chat.twitch.tv";
    int irc_port = 443; // Not 6667, just in case router blocks it.
    std::string irc_username = "";
    std::string irc_password = "oauth:<your_token_here>";
    std::string irc_channel = "";
    std::string irc_alias = "Twitch";
    Color irc_chat_color = Colors::RGB(0xAD,0x83,0xFA);

    bool show_messages = true;
    bool notify_on_user_leave = true;
    bool notify_on_user_join = true;

    bool isConnected() { return connected; };
    IRC* irc() { return &conn; };
private:
    bool pending_connect = false;
    bool pending_disconnect = false;
    bool connected = false;
    bool show_irc_password = false;
    bool twitch_enabled = true;

    void AddHooks();
    bool hooked = 0;

    char message_buffer[1024] = { 0 };

    IRC conn;

    GW::HookEntry SendChatCallback_Entry;
    GW::HookEntry StartWhisperCallback_Entry;
};
