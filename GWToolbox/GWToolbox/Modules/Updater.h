#pragma once

#include <string>

#include "ToolboxUIElement.h"

class Updater : public ToolboxUIElement {
	Updater() {};
	~Updater() {};
public:
	static Updater& Instance() {
		static Updater instance;
		return instance;
	}

	const char* Name() const override { return "Updater"; }

	void CheckForUpdate();
	void DoUpdate();

	void Draw(IDirect3DDevice9* device) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettings() override {};
	void DrawSettingInternal() override;

	std::string GetServerVersion() const { return server_version; }

private:
	std::string server_version = "";
	// 0=none, 1=check and warn, 2=check and ask, 3=check and do
	int mode = 0;

	// 0=checking, 1=asking, 2=downloading, 3=done
	enum Step {
		Checking,
		Asking,
		Downloading,
		Success,
		Done
	};
	Step step = Checking;

	std::string changelog = "";
};
