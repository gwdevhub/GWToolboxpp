#pragma once

#include <easywsclient\easywsclient.hpp>
#include <json.hpp>

#include <thread>
#include <cstdlib>
#include <iostream>
#include <list>

class TradeChat {
public:
	TradeChat();
	~TradeChat();

	std::vector <nlohmann::json> messages;
	std::vector <nlohmann::json> new_messages;

	void search(std::string);
	void fetch();
	void stop();
	bool is_active();
	bool is_connecting();
	bool is_timed_out();
private:
	enum Connection : unsigned int {
		not_connected,
		connecting,
		connected,
		timeout,
	};
	easywsclient::WebSocket::pointer ws = nullptr;
	Connection status = not_connected;
	int reconnect_attempt_max = 3;
	size_t max_messages = 100; // size_t fixes unsigned/signed warning when comparing to vector.size()
	std::thread connector;
	std::string base_uri = "wss://kamadan.decltype.org/ws/";

	void disconnect();
	void WSA_prereq();
	std::string ReplaceString(std::string, const std::string&, const std::string&);
};
