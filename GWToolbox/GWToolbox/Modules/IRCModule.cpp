#include "stdafx.h"
#include "Defines.h"

#include "IRCModule.h"
#include <WinSock2.h>
#include "logger.h"

#include "GuiUtils.h"

#include <GWCA/Packets/StoC.h>

#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\GameThreadMgr.h>
#include <GWCA\Managers\CtoSMgr.h>

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>
namespace {
    int OnJoin(const char* params, irc_reply_data* hostd, void* conn) {
        IRCModule* module = &IRCModule::Instance();
        if (!params[0] || !module->show_messages || !module->notify_on_user_join)
            return 0; // Empty msg
		if (strcmp(hostd->nick, module->irc_username.c_str()) == 0)
			return 0; // This is me.
        wchar_t buf[600];
        swprintf(buf, 599, L"<a=1>%S</a>: %S joined your channel.", module->irc_alias.c_str(), hostd->nick);
        GW::GameThread::Enqueue([buf]() {
            // NOTE: Messages are sent to the GWCA_1 channel - unused atm as far as i can see
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA1, buf);
            });
        return 0;
    }
    int OnLeave(const char* params, irc_reply_data* hostd, void* conn) {
        IRCModule* module = &IRCModule::Instance();
        if (!params[0] || !module->show_messages || !module->notify_on_user_leave)
            return 0; // Empty msg
        wchar_t buf[600];
        swprintf(buf, 599, L"<a=1>%S</a>: %S left your channel.", module->irc_alias.c_str(), hostd->nick);
        GW::GameThread::Enqueue([buf]() {
            // NOTE: Messages are sent to the GWCA_1 channel - unused atm as far as i can see
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA1, buf);
            });
        return 0;
    }
    int OnConnected(const char* params, irc_reply_data* hostd, void* conn) {
        IRCModule* module = &IRCModule::Instance();
        IRC* irc_conn = (IRC*)conn;
        char buf[128];
        Log::Log("%s: Connected %s", module->irc_alias.c_str(), params);
        sprintf(buf, "#%s", module->irc_channel.c_str());
        irc_conn->join(buf);
        return 0;
    }
    int OnMessage(const char* params, irc_reply_data* hostd, void* conn) {
        IRCModule* module = &IRCModule::Instance();
        if (!params[0] || !module->show_messages)
            return 0; // Empty msg
        wchar_t buf[600];
        swprintf(buf, 599, L"<a=1>%S @ %S</a>: %S", hostd->nick, module->irc_alias.c_str(), params);
        GW::GameThread::Enqueue([buf]() {
            // NOTE: Messages are sent to the GWCA_1 channel - unused atm as far as i can see
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA1, buf);
            });
        Log::Log("Message from %s: %s", hostd->nick, params);
        return 0;
    }
}
WSADATA wsaData; /* winsock stuff, linux/unix/*bsd users need not worry about this */
void IRCModule::Initialize() {
    ToolboxModule::Initialize();

    irc_server.resize(255);
    irc_username.resize(32);
    irc_password.resize(255);
    irc_channel.resize(56);

    conn.hook_irc_command("376", &OnConnected);
    conn.hook_irc_command("JOIN", &OnJoin);
    conn.hook_irc_command("PART", &OnLeave);
    conn.hook_irc_command("PRIVMSG", &OnMessage);
}
void IRCModule::AddHooks() {
	if (hooked) return;
	hooked = 1;
	// When starting a whisper to "<irc_nickname> @ <irc_channel>", rewrite recipient to be "<irc_channel>"
	GW::Chat::AddStartWhisperCallback([&](wchar_t* name) -> bool {
		wchar_t buf[128];
		std::wstring walias = GuiUtils::ToWstr(IRCModule::Instance().irc_alias);
		swprintf(buf, 128, L" @ %s", walias.c_str());
		if ((std::wstring(name)).find(buf) != -1) {
			wcscpy(name, walias.c_str());
		}
		return false;
	});
	// When sending a whisper to "<irc_channel>", redirect it to send message via IRC
	GW::Chat::AddSendChatCallback([&](GW::Chat::Channel chan, wchar_t* msg) -> bool {
		if (chan != GW::Chat::CHANNEL_WHISPER || !connected)
			return false;
		wchar_t msgcpy[255];
		wcscpy(msgcpy, msg);
		std::string message = GuiUtils::WStringToString(msgcpy);
		int sender_idx = message.find(',');
		if (sender_idx < 0)
			return false; // Invalid sender
		std::string to = message.substr(0, sender_idx);
		if (to.compare(irc_alias) != 0)
			return false;
		std::string content = message.substr(sender_idx + 1);
		Log::Log("Sending to IRC: %s", content.c_str());
		if (conn.raw("PRIVMSG #%s :%s\r\n", irc_channel.c_str(), content.c_str())) {
			Log::Error("Failed to send message");
		}
		else {
			irc_reply_data d;
			d.nick = const_cast<char*>(irc_username.c_str());
			OnMessage(content.c_str(), &d, &conn);
		}
		return true;
	});
}
void IRCModule::Terminate() {
    conn.disconnect();
    WSACleanup(); /* more winsock stuff */
}
bool IRCModule::Connect() {
    if (connected)
        return true;
    printf("Connecting to IRC\n");
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
        printf("Failed to initialise winsock!\n");
        return false;
    }
	if (irc_server.empty()) {
		printf("Invalid server name!\n");
		return false;
	}
	if(irc_username.empty()) {
        printf("Invalid username!\n");
        return false;
    }
    if (conn.start(
		const_cast<char*>(irc_server.c_str()),
		irc_port, 
		const_cast<char*>(irc_username.c_str()), 
		const_cast<char*>(irc_username.c_str()), 
		const_cast<char*>(irc_username.c_str()), 
		const_cast<char*>(irc_password.c_str())) != 0) {
        printf("IRC::start failed!\n");
        return false;
    }
    printf("Connected to IRC!\n");
    return connected = true;
}
void IRCModule::Update(float delta) {
    //conn.message_fetch(); // Check for messages
}
void IRCModule::DrawSettingInternal() {
    ImGui::Text("%s is used to exchange messages with an IRC Channel using the in-game chat window.",Name());
    ImGui::TextColored(ImColor(102, 187, 238, 255) , "What is IRC?");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Go to %s", "https://www.computerhope.com/jargon/i/irc.htm");
    if (ImGui::IsItemClicked())
        ShellExecute(NULL, "open", "https://www.computerhope.com/jargon/i/irc.htm", NULL, NULL, SW_SHOWNORMAL);
    if (ImGui::Button(connected ? "Disconnect" : "Connect", ImVec2(0, 0))) {
        if (connected) {
            conn.disconnect();
            connected = false;
        }
        else {
            Connect();
        }
    }
    float width = ImGui::GetContentRegionAvailWidth() / 2;
	ImGui::SameLine();
    ImGui::Checkbox("Show messages in chat window", &show_messages);
    ImGui::SameLine();
	if (Colors::DrawSettingHueWheel("Sender Color:", &irc_chat_color)) {
		GW::Chat::SetSenderColor(GW::Chat::Channel::CHANNEL_GWCA1, IRCModule::Instance().irc_chat_color);
	}
    ImGui::Checkbox("Notify on user leave", &notify_on_user_leave);
    ImGui::SameLine();
    ImGui::Checkbox("Notify on user join", &notify_on_user_join);
    ImGui::InputText("IRC Alias", const_cast<char*>(irc_alias.c_str()), 32);
    ImGui::ShowHelp("Sending a whisper to the IRC Alias will send the message to the IRC Channel.\nCannot contain spaces.");

    ImGui::InputText("IRC Server", const_cast<char*>(irc_server.c_str()), 255);
    ImGui::InputText("IRC Username", const_cast<char*>(irc_username.c_str()), 32);
    ImGui::InputText("IRC Password", const_cast<char*>(irc_password.c_str()), 255, show_irc_password ? 0 : ImGuiInputTextFlags_Password);
    ImGui::SameLine();
    ImGui::Checkbox("Show", &show_irc_password);    
    ImGui::InputText("IRC Channel", const_cast<char*>(irc_channel.c_str()), 56);
}
void IRCModule::LoadSettings(CSimpleIni* ini) {
    ToolboxModule::LoadSettings(ini);

    irc_alias = ini->GetValue(Name(), VAR_NAME(irc_alias), irc_alias.c_str());
    irc_server = ini->GetValue(Name(), VAR_NAME(irc_server), irc_server.c_str());
    irc_username = ini->GetValue(Name(), VAR_NAME(irc_username), irc_username.c_str());
    irc_password = ini->GetValue(Name(), VAR_NAME(irc_password), irc_password.c_str());
    irc_channel = ini->GetValue(Name(), VAR_NAME(irc_channel), irc_channel.c_str());

    show_messages = ini->GetBoolValue(Name(), VAR_NAME(show_messages), show_messages);
    notify_on_user_join = ini->GetBoolValue(Name(), VAR_NAME(notify_on_user_join), notify_on_user_join);
    notify_on_user_leave = ini->GetBoolValue(Name(), VAR_NAME(notify_on_user_leave), notify_on_user_leave);

    irc_chat_color = (GW::Chat::Color)Colors::Load(ini, Name(), VAR_NAME(irc_chat_color), irc_chat_color);
	GW::Chat::SetSenderColor(GW::Chat::Channel::CHANNEL_GWCA1, IRCModule::Instance().irc_chat_color);
	Color col1, col2;
	GW::Chat::GetChannelColors(GW::Chat::Channel::CHANNEL_GUILD, &col1, &col2);
	GW::Chat::SetMessageColor(GW::Chat::Channel::CHANNEL_GWCA1, col2);
	AddHooks();
	Connect();
}
void IRCModule::SaveSettings(CSimpleIni* ini) {
    ToolboxModule::SaveSettings(ini);

    ini->SetValue(Name(), VAR_NAME(irc_alias), irc_alias.c_str());
    ini->SetValue(Name(), VAR_NAME(irc_server), irc_server.c_str());
    ini->SetValue(Name(), VAR_NAME(irc_username), irc_username.c_str());
    ini->SetValue(Name(), VAR_NAME(irc_password), irc_password.c_str());
    ini->SetValue(Name(), VAR_NAME(irc_channel), irc_channel.c_str());

    ini->SetBoolValue(Name(), VAR_NAME(show_messages), show_messages);
    ini->SetBoolValue(Name(), VAR_NAME(notify_on_user_join), notify_on_user_join);
    ini->SetBoolValue(Name(), VAR_NAME(notify_on_user_leave), notify_on_user_leave);

    Colors::Save(ini, Name(), VAR_NAME(irc_chat_color), irc_chat_color);
}