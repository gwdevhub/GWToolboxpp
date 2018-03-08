#include "TradeChat.h"

#include <iostream>
#include <thread>
#include <WinSock2.h>

#include <logger.h>

TradeChat::TradeChat() {
	INT rc;
	WSADATA wsaData;

	rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (rc) {
		printf("WSAStartup Failed.\n");
		return;
	}

	ws = easywsclient::WebSocket::from_url("wss://kamadan.decltype.org/ws/");
	ws_active = true;
}

void TradeChat::fetch(){
	new_messages.clear();
	ws->poll();
	ws->dispatch([this](const std::string & message) {
		nlohmann::json chat_json = nlohmann::json::parse(message.c_str());
		if (chat_json.is_object()) {
			if (chat_json["results"].is_array()) {
				for (nlohmann::json::iterator it = chat_json["results"].begin(); it != chat_json["results"].end(); ++it) {
					new_messages.insert(new_messages.begin(), *it);
					messages.insert(messages.begin(), *it);
				}
			}
			else {
				new_messages.insert(new_messages.begin(), chat_json);
				messages.insert(messages.begin(), chat_json);
			}
		}
			
		while (messages.size() > 25) {
			messages.pop_back();
		}
	});
}

void TradeChat::stop() {
	if (ws_active) {
		Log::Log("Destroying connection to trade chat\n");
		delete ws;
		WSACleanup();
		ws_active = false;
	}
}

TradeChat::~TradeChat() {
	stop();
}
