#include "ChatCommands.h"

#include "PconPanel.h"
#include "GWToolbox.h"

using namespace std;

ChatCommands::ChatCommands() {
	GWCA api;

	api->Chat()->RegisterKey(
		L"pcons", std::bind(&ChatCommands::PconCmd, this, std::placeholders::_1));

	api->Chat()->SetColor(0xDDDDDD);
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
