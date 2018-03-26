#pragma once

#include <ToolboxWindow.h>
#include <iostream>
#include <vector>
#include <set>

#include <queue>
#include <thread>

#include <json.hpp>
#include <easywsclient\easywsclient.hpp>
#include <CircurlarBuffer.h>

class TradeWindow : public ToolboxWindow {
	struct Alert {
		static unsigned int uid_count;
		Alert(const char* match = "") {
			strncpy(match_string, match, 128);
			uid = uid_count++;
		}
		char match_string[128];
		unsigned int uid;
	};

	TradeWindow() {};
	~TradeWindow() {};
public:
	static TradeWindow& Instance() {
		static TradeWindow instance;
		return instance;
	}

	const char* Name() const { return "Trade"; }

	void Initialize() override;
	void Terminate() override;

	void Update(float delta) override;
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

private:
    struct Message {
        uint32_t    timestamp;
        std::string name;
        std::string message;
    };

	// if the player has an alert with exactly this keyword, all messages will be matched
	bool alert_all = false;
	std::string chat_color = "f96677";
	// buffer for the search input
	char search_buffer[256];
	#define ALERT_BUF_SIZE 1024 * 16
	char alert_buf[ALERT_BUF_SIZE];
	bool alertfile_dirty = false;
	wchar_t* alertfilename = L"AlertKeywords.txt";
	std::set<std::string> alerts;
	bool show_alert_window = false;

    // if we need to print in the chat
    bool print_chat = false;

    // Since we are connecting in an other thread, the following attributes/methods avoid spamming connection requests
    void AsyncChatConnect();
    void AsyncWindowConnect();
    bool ws_chat_connecting = false;
    bool ws_window_connecting = false;

    easywsclient::WebSocket *ws_chat = NULL;
    easywsclient::WebSocket *ws_window = NULL;

    bool search_pending;
    void search(std::string);
    void fetch();

    static Message parse_json_message(nlohmann::json js);
    CircularBuffer<Message> messages;

    // tasks to be done async by the worker thread
	std::queue<std::function<void()>> thread_jobs;
    bool should_stop = false;
	std::thread worker;

	std::string alerts_tooltip = \
		"Trade messages with matched keywords will be send to the Guild Wars Trade chat.\n" \
		"Each line is a new keyword and the keywords are not case sensitive.\n" \
		"The Trade checkbox in the Guild Wars chat must be selected for messages to show up.\n";

	std::string ReplaceString(std::string, const std::string&, const std::string&);
	void ParseBuffer(const char *, std::set<std::string>&);
};
