#pragma once

#include "IRC.h"
#include <ToolboxModule.h>

#include <GWCA/Utilities/Hook.h>

#include <Color.h>

class IRCModule : public ToolboxModule {
    IRCModule() {};
    ~IRCModule() {};
public:
    static IRCModule& Instance() {
        static IRCModule instance;
        return instance;
    }

    const char* Name() const override { return "Twitch Chat"; }
    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;
    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
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
    bool connected = false;
    bool show_irc_password = false;

	void AddHooks();
	bool hooked = 0;

    char message_buffer[1024] = { 0 };

    IRC conn;

	GW::HookEntry SendChatCallback_Entry;
	GW::HookEntry StartWhisperCallback_Entry;
};