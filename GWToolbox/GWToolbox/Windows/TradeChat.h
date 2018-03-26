#pragma once

#include <easywsclient\easywsclient.hpp>

#include <vector>
#include <thread>
#include <stdint.h>
#include <CircurlarBuffer.h>

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
    void dismiss();
    void fetchAll();

    // temp: used to print only the new message to the char.
    size_t append_count;

	Status status = disconnected;
    CircularBuffer<Message> messages;

private:
    std::thread thread;
    void onMessage(const std::string& msg);
	easywsclient::WebSocket *ws = NULL;
    bool search_pending = false;
};
