#pragma once

#include <vector>

#include <ToolboxWindow.h>

class MainWindow : public ToolboxWindow {
	MainWindow() {};
	~MainWindow() {};
public:
	static MainWindow& Instance() {
		static MainWindow instance;
		return instance;
	}

	const char* Name() const { return "Toolbox"; }

	void Initialize() override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

private:
	bool one_panel_at_time_only = false;
};
