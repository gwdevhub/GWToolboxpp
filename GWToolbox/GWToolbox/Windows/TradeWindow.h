#pragma once

#include "TradeChat.h"

#include <ToolboxWindow.h>
#include <iostream>
#include <vector>
#include <thread>

class TradeWindow : public ToolboxWindow {
	struct Alert {
		static unsigned int uid_count;
		Alert(const char* match = "") {
			strncpy(match_string, match, 128);
			uid = uid_count++;
		}
		char match_string[128];
		unsigned int uid;
	};

	TradeWindow() {};
	~TradeWindow() {};
public:
	static TradeWindow& Instance() {
		static TradeWindow instance;
		return instance;
	}

	const char* Name() const { return "Trade"; }

	void Initialize() override;
	void Terminate() override;

	void Update(float delta) override;
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadAlerts();
	void SaveAlerts();
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

private:
	// if the player has an alert with exactly this keyword, all messages will be matched
	std::string all_keyword = "ALL";
	std::string chat_color = "f96677";
	// buffer for the search input
	char search_buffer[256];
	CSimpleIni* alert_ini = nullptr;
	std::wstring ini_filename = L"trade_alerts.ini";
	std::vector<Alert> alerts;
	bool show_alert_window = false;
	TradeChat* all_trade = nullptr;
	TradeChat* trade_searcher = nullptr;

	std::string alerts_tooltip = \
		"Click to add a new keyword.\n" \
		"\t- Trade messages with matched keywords will be send to the Guild Wars chat.\n" \
		"\t- The keywords are not case sensitive.\n" \
		"\t- The Trade checkbox in the Guild Wars chat must be selected for messages to show up.\n" \
		"\t- To match every message, create an alert with the only keyword: ALL";
};