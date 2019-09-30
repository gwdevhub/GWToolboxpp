#pragma once

#include "IRC.h"
#include <ToolboxModule.h>

#include <Color.h>

class IRCModule : public ToolboxModule {
    IRCModule() {};
    ~IRCModule() {};
public:
    static IRCModule& Instance() {
        static IRCModule instance;
        return instance;
    }

    const char* Name() const override { return "IRC Integration"; }
    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;
    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;
    bool Connect();
    // IRC details
    std::string irc_server="irc.chat.twitch.tv";
    int irc_port = 443; // Not 6667, just in case router blocks it.
    std::string irc_username = "<your_twitch_username>";
    std::string irc_password = "oauth:<your_token_here>";
    std::string irc_channel = "<twitch_streamer_username>";
    std::string irc_alias = "Twitch";
    Color irc_chat_color = Colors::RGB(0x9B,0x66,0xFF);

    bool show_messages = true;
    bool notify_on_user_leave = true;
    bool notify_on_user_join = true;

    bool isConnected() { return connected; };
    IRC* irc() { return &conn; };
private:
    bool pending_connect = false;
    bool connected = false;
    bool show_irc_password = false;

	void AddHooks();
	bool hooked = 0;

    char message_buffer[1024] = { 0 };

    IRC conn;
};