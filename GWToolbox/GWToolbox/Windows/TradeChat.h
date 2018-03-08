#pragma once

#include <easywsclient\easywsclient.hpp>
#include <json.hpp>

#include <thread>
#include <cstdlib>
#include <iostream>
#include <list>


class TradeChat {
public:
	static TradeChat& Instance() {
		static TradeChat instance;
		return instance;
	}

	TradeChat();
	~TradeChat();
	std::string latest;
	std::string latest_name;
	void search(std::string);
	void fetch();
	void stop();
	bool is_active();
	std::vector <nlohmann::json> new_messages;
	std::vector <nlohmann::json> messages;

private:
	enum Connection : unsigned int {
		not_connected = 0,
		connecting = 1,
		connected = 2,
	};
	Connection status;
	std::thread connector;
	easywsclient::WebSocket::pointer ws = NULL;
	std::string uri = "wss://kamadan.decltype.org/ws/";	

	void stop_current();
};