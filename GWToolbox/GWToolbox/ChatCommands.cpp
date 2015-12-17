#include "ChatCommands.h"

#include <algorithm>

#include "PconPanel.h"
#include "GWToolbox.h"
#include "ChatLogger.h"

using namespace std;

ChatCommands::ChatCommands() {

	ChatLogger::Init();

	AddCommand(L"age2", ChatCommands::CmdAge2);
	AddCommand(L"pcons", ChatCommands::CmdPcons);
	AddCommand(L"dialog", ChatCommands::CmdDialog);
	AddCommand(L"tb", ChatCommands::CmdTB);
	AddCommand(L"tp", ChatCommands::CmdTP);
	AddCommand(L"to", ChatCommands::CmdTP);
	AddCommand(L"travel", ChatCommands::CmdTP);
}

void ChatCommands::AddCommand(wstring cmd, Handler_t handler) {
	GWCA::Api().Chat().RegisterCommand(cmd,
		[handler](std::wstring cmd, std::vector<std::wstring> args) {
		handler(args);
	});
}

wstring ChatCommands::GetLowerCaseArg(vector<wstring> args, int index) {
	wstring arg = args[index];
	std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
	return arg;
}

void ChatCommands::CmdAge2(vector<wstring>) {
	wchar_t buffer[30];
	GWCA api;
	DWORD second = api().Map().GetInstanceTime() / 1000;
	wsprintf(buffer, L"%02u:%02u:%02u", (second / 3600), (second / 60) % 60, second % 60);
	ChatLogger::Log(buffer);
}

void ChatCommands::CmdPcons(vector<wstring> args) {
	PconPanel& pcons = GWToolbox::instance().main_window().pcon_panel();
	if (args.size() == 0) {
		pcons.ToggleActive();
	} else { // we are ignoring parameters after the first
		wstring arg = GetLowerCaseArg(args, 0);
		if (arg == L"on") {
			pcons.SetActive(true);
		} else if (arg == L"off") {
			pcons.SetActive(false);
		} else {
			ChatLogger::LogF(L"[Error] Invalid argument '%ls', please use /pcons [|on|off]", args[0].c_str());
		}
	}
}

void ChatCommands::CmdDialog(vector<wstring> args) {
	if (args.size() == 0) {
		ChatLogger::LogF(L"[Error] Please provide an integer or hex argument");
	} else {
		try {
			long id = std::stol(args[0], 0, 0);
			GWCA::Api().Agents().Dialog(id);
			ChatLogger::LogF(L"Sent Dialog 0x%X", id);
		} catch (...) {
			ChatLogger::LogF(L"[Error] Invalid argument '%ls', please use an integer or hex value", args[0].c_str());
		}
	}
}

void ChatCommands::CmdTB(vector<wstring> args) {
	if (args.size() == 0) {
		ChatLogger::Log(L"[Error] Please provide an argument");
	} else {
		wstring arg = GetLowerCaseArg(args, 0);
		if (arg == L"age") {
			CmdAge2(args);
		} else if (arg == L"hide") {
			GWToolbox::instance().main_window().SetVisible(false);
		} else if (arg == L"show") {
			GWToolbox::instance().main_window().SetVisible(true);
		} else if (arg == L"reset") {
			GWToolbox::instance().main_window().SetLocation(0, 0);
		} else if (arg == L"mini" || arg == L"minimize") {
			GWToolbox::instance().main_window().SetMinimized(true);
		} else if (arg == L"maxi" || arg == L"maximize") {
			GWToolbox::instance().main_window().SetMinimized(false);
		}
	}
}

void ChatCommands::CmdTP(vector<wstring> args) {
	if (args.size() == 0) {
		ChatLogger::Log(L"[Error] Please provide an argument");
	} else {
		wstring arg = GetLowerCaseArg(args, 0);
		if (arg == L"toa") {
			GWCA::Api().Map().Travel(GwConstants::MapID::Temple_of_the_Ages);
		} else if (arg == L"doa") {
			GWCA::Api().Map().Travel(GwConstants::MapID::Domain_of_Anguish);
		} else if (arg == L"kamadan" || arg == L"kama") {
			GWCA::Api().Map().Travel(GwConstants::MapID::Kamadan_Jewel_of_Istan_outpost);
		} else if (arg == L"embark") {
			GWCA::Api().Map().Travel(GwConstants::MapID::Embark_Beach);
		} else if (arg == L"vlox" || arg == L"vloxs") {
			GWCA::Api().Map().Travel(GwConstants::MapID::Vloxs_Falls);
		} else if (arg == L"gadd" || arg == L"gadds") {
			GWCA::Api().Map().Travel(GwConstants::MapID::Gadds_Encampment_outpost);
		} else if (arg == L"urgoz") {
			GWCA::Api().Map().Travel(GwConstants::MapID::Urgozs_Warren);
		} else if (arg == L"deep") {
			GWCA::Api().Map().Travel(GwConstants::MapID::The_Deep);
		} else if (arg == L"fav1") {
			GWToolbox::instance().main_window().travel_panel().TravelFavorite(0);
		} else if (arg == L"fav2") {
			GWToolbox::instance().main_window().travel_panel().TravelFavorite(1);
		} else if (arg == L"fav3") {
			GWToolbox::instance().main_window().travel_panel().TravelFavorite(2);
		} else if (arg == L"gh") {
			GWCA::Api().Guild().TravelGH();
		}
	}
}
