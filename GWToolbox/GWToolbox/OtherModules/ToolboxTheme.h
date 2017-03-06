#pragma once

#include <ToolboxModule.h>

class ToolboxTheme : public ToolboxModule {
	ToolboxTheme();
	~ToolboxTheme() {};
public:
	static ToolboxTheme& Instance() {
		static ToolboxTheme instance;
		return instance;
	}

	const char* Name() const override { return "Theme"; }

	void Terminate() override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

private:
	ImGuiStyle DefaultTheme();

	ImGuiStyle ini_style;
	CSimpleIni* inifile = nullptr;
};
