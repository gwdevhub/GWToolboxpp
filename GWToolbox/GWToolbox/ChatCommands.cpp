#include "ChatCommands.h"

#include "PconPanel.h"
#include "GWToolbox.h"

using namespace std;

ChatCommands::ChatCommands() {
	GWCA api;

	api().Chat().RegisterChannel(L"GWToolbox++", 0x00CCFF, 0xDDDDDD);

	api().Chat().RegisterCommand(L"pcons", 
		std::bind(&ChatCommands::PconCmd, this, std::placeholders::_1));

	api().Chat().RegisterCommand(L"age2", [api](vector<wstring>&){ // Exemple of new channel system
		wchar_t buffer[30];
		DWORD second = api().Map().GetInstanceTime() / 1000;

		wsprintf(buffer, L"%02u:%02u:%02u", (second / 3600), (second / 60) % 60, second % 60);

		api().Chat().WriteChat(L"GWToolbox++", buffer);
	});
}

void ChatCommands::PconCmd(const vector<wstring> &args) {

	PconPanel& pcons = GWToolbox::instance().main_window().pcon_panel();
	if (args.size() == 0) {
		pcons.ToggleActive();
	}

	if (args.size() == 1) {
		if (args[0].compare(L"on") == 0) {
			pcons.SetActive(true);
		} else if (args[0].compare(L"off") == 0) {
			pcons.SetActive(false);
		}
	}
}
