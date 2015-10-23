#include "ChatCommands.h"

#include "PconPanel.h"
#include "GWToolbox.h"

using namespace std;

ChatCommands::ChatCommands() {
	GWCA api;

	api->Chat()->RegisterKey(L"pcons", 
		std::bind(&ChatCommands::PconCmd, this, std::placeholders::_1));

	wstring channel1 = api->Chat()->CreateChannel(wstring(L"~~%T~~"));

	wstring channel2 = api->Chat()->CreateChannel([](wstring msg) {
		return msg + L"rofl";
	});

	api->Chat()->RegisterKey(L"test",
		[channel1](vector<wstring>) {
		GWCA api;
		api->Chat()->WriteChat(channel1.c_str(), L"Hello World!");
	});

	api->Chat()->RegisterKey(L"test2",
		[channel2](vector<wstring>) {
		GWCA api;
		api->Chat()->WriteChat(channel2.c_str(), L"Bye World!");
	});
}

void ChatCommands::PconCmd(vector<wstring> args) {

	PconPanel* pcons = GWToolbox::instance()->main_window()->pcon_panel();
	if (args.size() == 0) {
		pcons->ToggleActive();
	}

	if (args.size() == 1) {
		if (args[0].compare(L"on") == 0) {
			pcons->SetActive(true);
		} else if (args[0].compare(L"off") == 0) {
			pcons->SetActive(false);
		}
	}
}
