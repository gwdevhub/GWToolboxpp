#include "TradeChat.h"

#pragma warning(push)
#pragma warning(disable: 4503 4996)

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
	ws->poll();
	ws->dispatch([this](const std::string & message) {
		nlohmann::json chat_json = nlohmann::json::parse(message.c_str());
		if (chat_json.is_object()) {
			if (chat_json["results"].is_array()) {
				for (nlohmann::json::iterator it = chat_json["results"].begin(); it != chat_json["results"].end(); ++it) {
					messages.insert(messages.begin(), *it);
				}
			}
			else {
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

#pragma warning(pop)