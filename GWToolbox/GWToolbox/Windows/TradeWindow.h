#pragma once

#include "TradeChat.h"

#include <ToolboxWindow.h>
#include <iostream>
#include <vector>
#include <thread>

class TradeWindow : public ToolboxWindow {
	TradeWindow() {};
	~TradeWindow() {};
public:
	static TradeWindow& Instance() {
		static TradeWindow instance;
		return instance;
	}

	struct Alert {
		static unsigned int uid_count;
		Alert(const char* match = "") {
			strncpy(match_string, match, 128);
			uid = uid_count++;
		}
		char match_string[128];
		unsigned int uid;
	};

	const char* Name() const { return "Trade"; }

	void Initialize() override;
	void Terminate() override;

	void Update(float delta) override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;


	void LoadAlerts();
	void SaveAlerts();
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

private:
	CSimpleIni* alert_ini = nullptr;
	std::string ini_filename = "trade_alerts.ini";
	std::vector<Alert> alerts;
	bool show_alert_window = false;
	TradeChat chat;
};