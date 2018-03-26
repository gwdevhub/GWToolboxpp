#pragma once

#include <easywsclient\easywsclient.hpp>

#include <vector>
#include <thread>
#include <stdint.h>

class TradeChat {
public:
	TradeChat();
	~TradeChat();

    struct Message {
        uint32_t    timestamp;
        std::string name;
        std::string message;
    };

   enum Status {
		disconnected,
		connecting,
		connected,
	};

    void connectAsync();
    void connect();
    void close();

	void search(std::string);
    void fetchAll();
    void dismiss();

	Status status = disconnected;
    std::vector<Message> messages;
    std::vector<Message> queries;

private:
    std::thread thread;
    void onMessage(const std::string& msg);
	easywsclient::WebSocket *ws = NULL;
    bool search_pending = false;
};
