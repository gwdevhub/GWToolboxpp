#include "TradeChat.h"

#include <string>
#include <algorithm>
#include <functional>

#include <WinSock2.h>

#include <logger.h>
#include <json.hpp>

using namespace easywsclient;
using namespace nlohmann;

using json_vec = std::vector<json>;

std::string TradeChat::ReplaceString(std::string subject, const std::string& search, const std::string& replace) {
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != std::string::npos) {
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
	return subject;
}

TradeChat::TradeChat() {
	WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("[Error] Couldn't initialize Winsock2 (%lu)\n", WSAGetLastError());
        return;
    }
}

void TradeChat::connect() {
    const char host[] = "wss://kamadan.decltype.org/ws/";
    if (!(ws = WebSocket::from_url(host))) {
        printf("Couldn't connect to the host '%s'", host);
        return;
    }

    status = connected;
}

void TradeChat::close() {
    if (ws != NULL) {
        ws->close();
        ws = NULL;
    }
    status = disconnected;
}

void TradeChat::search(std::string query) {
    static std::string search_uri = "wss://kamadan.decltype.org/ws/search";

    if (search_pending)
        return;
    search_pending = true;
    
    // This function has terrible performance.
	query = ReplaceString(query, " ", "%20");
	std::string uri = search_uri + query;

    /*
     * The protocol is the following:
     *  - From a connected web socket, we send a Json formated packet with the format
     *  {
     *   "query":  str($query$),
     *   "offset": int($page$),
     *   "sugest": int(1 or 2)
     *  }
     */

    json request;
    request["query"] = query;
    request["offset"] = 0;
    request["suggest"] = 0;
    ws->send(request.dump());
}

static void do_nothing(const std::string& msg)
{
}

void TradeChat::dismiss() {
    assert(ws && ws->getReadyState() == WebSocket::OPEN);
    ws->poll();
    ws->dispatch(do_nothing);
}

void TradeChat::fetchAll() {
    assert(ws && ws->getReadyState() == WebSocket::OPEN);

    messages.clear();
    ws->poll();
    ws->dispatch(std::bind(&TradeChat::onMessage, this, std::placeholders::_1));
}

static TradeChat::Message parse_json_message(json js)
{
    TradeChat::Message msg;
    msg.name = js["name"].get<std::string>();
    msg.message = js["message"].get<std::string>();
    msg.timestamp = js["timestamp"].get<std::string>();
    return msg;
}

void TradeChat::onMessage(const std::string& msg) {
    json res = json::parse(msg);

    if (res.find("query") == res.end()) {
        // TODO: Handle op, etc etc...
        Message msg = parse_json_message(res);
        messages.push_back(msg);
        return;
    }
    
    search_pending = false;
    if (res.find("results") == res.end())
        return;

    json_vec results = res["results"].get<json_vec>();
    queries.clear();
    queries.reserve(results.size());

    for (auto &it : results) {
        Message msg = parse_json_message(it);
        queries.push_back(msg);
    }
}

TradeChat::~TradeChat() {
    close();
    WSACleanup();
}
