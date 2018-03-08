#pragma once

#include <easywsclient\easywsclient.hpp>
#include <json.hpp>

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
	void fetch();
	void stop();
	std::vector <nlohmann::json> new_messages;
	std::vector <nlohmann::json> messages;

private:
	bool ws_active = false;
	bool should_stop = false;
	easywsclient::WebSocket::pointer ws = NULL;
	std::string uri = "wss://kamadan.decltype.org/ws/";	
};