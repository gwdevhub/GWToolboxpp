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
	WSA_prereq();
}

// strange name as I didn't want to overlap any existing WSA function names
void TradeChat::WSA_prereq() {
	INT rc;
	WSADATA wsaData;

	rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (rc) {
		printf("WSAStartup Failed.\n");
		return;
	}
}

void TradeChat::search(std::string search_string) {
	if (status == connecting) return;
	search_string = ReplaceString(search_string, " ", "%20");
	disconnect();
	messages.clear();
	status = connecting;
	connector = std::thread([this, search_string]() {
		std::string uri_with_search = search_string.empty() ? base_uri : base_uri + "search/" + search_string;
		this->ws = easywsclient::WebSocket::from_url(uri_with_search);
		this->status = connected;
	});
}

void TradeChat::fetch(){
	if (status != connected) return;
	new_messages.clear();
	ws->poll();
	ws->dispatch([this](const std::string & message) {
		nlohmann::json chat_json = nlohmann::json::parse(message.c_str());
		if (chat_json.is_object()) {
			if (chat_json["results"].is_array()) {
				for (nlohmann::json::iterator it = chat_json["results"].begin(); it != chat_json["results"].end(); ++it) {
					messages.push_back(*it);
				}
			}
			else {
				new_messages.insert(new_messages.begin(), chat_json);
				messages.insert(messages.begin(), chat_json);
			}
		}
		while (messages.size() > max_messages) {
			messages.pop_back();
		}
	});
}

void TradeChat::disconnect() {
	if (status != not_connected) {
		Log::Log("Destroying connection to trade chat\n");
		delete ws;
		connector.join();
		status = not_connected;
	}
}

void TradeChat::stop() {
	while (status == connecting) {}
	disconnect();
	WSACleanup();
}

bool TradeChat::is_active() {
	return status != not_connected;
}

TradeChat::~TradeChat() {
	stop();
}
