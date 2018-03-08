#include "TradeChat.h"

#include <iostream>
#include <thread>
#include <string>
#include <algorithm>
#include <WinSock2.h>

#include <logger.h>

// https://stackoverflow.com/questions/5343190/how-do-i-replace-all-instances-of-a-string-with-another-string
std::string ReplaceString(std::string subject, const std::string& search, const std::string& replace) {
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != std::string::npos) {
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
	return subject;
}

TradeChat::TradeChat() {
	INT rc;
	WSADATA wsaData;

	rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (rc) {
		printf("WSAStartup Failed.\n");
		return;
	}
	search("");
}

void TradeChat::search(std::string search_string=nullptr) {
	if (status == connecting) return;
	search_string = ReplaceString(search_string, " ", "%20");
	stop_current();
	status = connecting;
	connector = std::thread([this, search_string]() {
		std::string uri_with_search = search_string.empty() ? uri : uri + "search/" + search_string;
		std::cout << uri_with_search << std::endl;
		this->ws = easywsclient::WebSocket::from_url(uri_with_search);

		this->status = connected;
	});
}

void TradeChat::fetch(){
	new_messages.clear();
	if (status != connected) return;
	ws->poll();
	ws->dispatch([this](const std::string & message) {
		nlohmann::json chat_json = nlohmann::json::parse(message.c_str());
		if (chat_json.is_object()) {
			if (chat_json["results"].is_array()) {
				for (nlohmann::json::iterator it = chat_json["results"].begin(); it != chat_json["results"].end(); ++it) {
					//new_messages.insert(new_messages.begin(), *it);
					messages.push_back(*it);
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

void TradeChat::stop_current() {
	if (status != not_connected) {
		Log::Log("Destroying connection to trade chat\n");
		messages.clear();
		delete ws;
		connector.join();
		status = not_connected;
	}
}

void TradeChat::stop() {
	while (status == connecting) {}
	stop_current();
	WSACleanup();
}

bool TradeChat::is_active() {
	return status != not_connected;
}

TradeChat::~TradeChat() {
	stop();
}
