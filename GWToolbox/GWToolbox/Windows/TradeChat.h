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
private:
	enum Connection : unsigned int {
		not_connected = 0,
		connecting = 1,
		connected = 2,
	};
	easywsclient::WebSocket::pointer ws;
	Connection status = not_connected;
	size_t max_messages = 100; // size_t fixes unsigned/signed mismatch when comparing to vector.size()
	std::thread connector;
	std::string base_uri = "wss://kamadan.decltype.org/ws/";

	void disconnect();
	void WSA_prereq();
};