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

#include <GWCA\Utilities\Hook.h>

#include "Utils\RateLimiter.h"

class TradeWindow : public ToolboxWindow {
	TradeWindow() {};
	~TradeWindow();
public:
	static TradeWindow& Instance() {
		static TradeWindow instance;
		return instance;
	}

	const char* Name() const { return "Trade"; }

	void Initialize() override;
	void Terminate() override;

	bool FilterTradeMessage(std::wstring message);
	bool FilterTradeMessage(std::string message);

	void Update(float delta) override;
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;


private:
	struct Message {
		time_t    timestamp = 0;
		std::string name;
		std::string message;
		inline bool contains(std::string search) {
			auto it = std::search(
				message.begin(), message.end(),
				search.begin(), search.end(),
				[](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
			);
			return (it != message.end());
		};
	};

	std::regex word_regex;
	std::smatch m;

	bool show_alert_window = false;

	bool enable_kamadan_decltype = true;
	// if we need to print in the chat
	bool print_game_chat = false;
	// Flash window when trade alert matches
	bool flash_window_on_trade_alert = false;

	// if enable, we won't print the messages containing word from alert_words
	bool filter_alerts = false;
	bool only_show_trade_alerts_in_kamadan = false;

#define ALERT_BUF_SIZE 1024 * 16
	char alert_buf[ALERT_BUF_SIZE] = "";
	// set when the alert_buf was modified
	bool alertfile_dirty = false;
	bool localtradelogfile_dirty = false;

	std::vector<std::string> alert_words;

	void DrawAlertsWindowContent(bool ownwindow);

	void NotifyTradeBlockedInKamadan();

	static bool GetInKamadan();
	static bool GetInKamadanAE1();

	// Since we are connecting in an other thread, the following attributes/methods avoid spamming connection requests
	void AsyncChatConnect(bool force = false);
	void AsyncWindowConnect(bool force = false);
	void ConnectionFailed(bool forced = false);

	bool ws_chat_connecting = false;
	bool ws_window_connecting = false;

	RateLimiter chat_rate_limiter;
	RateLimiter window_rate_limiter;

	easywsclient::WebSocket* ws_chat = NULL;
	easywsclient::WebSocket* ws_window = NULL;

	// When was last connect attempted?
	clock_t socket_retry_clock = 0;
	// Exponential backoff for failed connection
	long socket_retry_interval = 0;
	// Number of connection fails.
	unsigned int socket_fail_count = 0;
	// Have we told the user?
	bool logged_socket_fail_error = false;

	bool search_pending;
	void search(std::string);
	void search_local(std::string);
	bool searching = true;
	void fetch();

	CSimpleIni* trade_log_ini;

	static Message parse_json_message(nlohmann::json js);
	// Messages from kamadan.decltype.org
	CircularBuffer<Message> messages;
	// Messages from within kamadan ae1, used as a fallback. Records a log of ALL trade messages, not just searched ones.
	CircularBuffer<Message> outpost_messages;
	// Used for searching outpost_messages
	CircularBuffer<Message> outpost_messages_tmp;
	// 2 copies of this to allow search to run on another thread without locking Draw()
	CircularBuffer<Message> outpost_messages_filtered_a;
	CircularBuffer<Message> outpost_messages_filtered_b;
	CircularBuffer<Message>* outpost_messages_filtered_ptr;
	std::string searched_message;

	// tasks to be done async by the worker thread
	std::queue<std::function<void()>> thread_jobs;
	bool should_stop = false;
	std::thread worker;

	char search_buffer[256];

	void ParseBuffer(const char* text, std::vector<std::string>& words);
	void ParseBuffer(std::fstream stream, std::vector<std::string>& words);

	static void DeleteWebSocket(easywsclient::WebSocket* ws);

	GW::HookEntry MessageLocal_Entry;
	GW::HookEntry OnTradeMessage_Entry;
};
