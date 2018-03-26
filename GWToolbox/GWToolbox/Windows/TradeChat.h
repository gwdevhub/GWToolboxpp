#pragma once

#include <easywsclient\easywsclient.hpp>

#include <vector>
#include <stdint.h>

class TradeChat {
public:
	TradeChat();
	~TradeChat();

    struct Message {
        std::string timestamp;
        std::string name;
        std::string message;
    };

    void connect();
    void close();

	void search(std::string);
    void fetchAll();
    void dismiss();

    std::vector<Message> messages;
    std::vector<Message> queries;
private:
	enum Status {
		disconnected,
		connecting,
		connected,
		timeout,
	};

    void onMessage(const std::string& msg);

	easywsclient::WebSocket *ws;
	Status status = disconnected;

    bool search_pending = false;

	std::string ReplaceString(std::string, const std::string&, const std::string&);
};
