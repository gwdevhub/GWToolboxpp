#include "ChatCommands.h"

#include "PconPanel.h"
#include "GWToolbox.h"

using namespace std;

ChatCommands::ChatCommands() {
	//GWCA::Api().Chat().RegisterChannel(L"GWToolbox++", 0x00CCFF, 0xDDDDDD);
	GWCA::Api().Chat().CreateChannel(std::wstring(L"{GWToolbox++}%T"));

	AddCommand(L"age2", [](vector<wstring>) {
		wchar_t buffer[30];
		GWCA api;
		DWORD second = api().Map().GetInstanceTime() / 1000;
		wsprintf(buffer, L"%02u:%02u:%02u", (second / 3600), (second / 60) % 60, second % 60);
		api().Chat().WriteChat(L"GWToolbox++", buffer);
		return true;
	});

	AddCommand(L"pcons", std::bind(&ChatCommands::PconCmd, this, std::placeholders::_1));

	// sleep ms
	// waits for ms milliseconds
	AddCommand(L"sleep", [](vector<wstring> args) {
		if (args.size() < 1) {
			printf("sleep requires one argument (time in milliseconds)");
			return false;
		}
		try {
			long time = std::stoi(args[0]);
			Sleep(time);
			return true;
		} catch (std::invalid_argument e) {
			printf("sleep requires an integer as its argument\n");
			return false;
		} catch (std::out_of_range e) {
			printf("argument %S out of range\n");
			return false;
		}
	});
}

void ChatCommands::AddCommand(wstring cmd, Handler_t handler) {
	if (commands_.find(cmd) != commands_.end()) {
		printf("[WARNING] Adding command %S which is already in the map!\n", cmd.c_str());
	}
	commands_[cmd] = handler;
	GWCA::Api().Chat().RegisterKey(cmd, [handler](std::vector<std::wstring> args){
		handler(args);
	});
}

void ChatCommands::ParseCommand(wstring cmd, vector<wstring> args) {
	if (commands_.find(cmd) != commands_.end()) {
		bool ret = commands_[cmd](args);
	}
}

bool ChatCommands::PconCmd(vector<wstring> args) {
	PconPanel& pcons = GWToolbox::instance().main_window().pcon_panel();
	if (args.size() == 0) {
		pcons.ToggleActive();
	} else if (args.size() == 1) {
		if (args[0].compare(L"on") == 0) {
			pcons.SetActive(true);
		} else if (args[0].compare(L"off") == 0) {
			pcons.SetActive(false);
		} else {
			return false;
		}
	}
	return true;
}
