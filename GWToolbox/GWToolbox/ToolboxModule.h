#pragma once

#include <imgui.h>
#include <DirectX\Include\d3d9.h>
#include <SimpleIni.h>

class ToolboxModule {
public:
	ToolboxModule() {}
	virtual ~ToolboxModule() {};

	// name of the window and the ini section
	virtual const char* Name() const = 0;

	// Update. Will always be called once every frame.
	virtual void Update() {};

	// Draw user interface. Will be called every frame if the element is visible
	virtual void Draw(IDirect3DDevice9* pDevice) {};

	// This is provided (and called), but use ImGui::GetIO() during update/render if possible instead.
	virtual bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) { return false; };

	// Draw settings interface. Will be called if the setting panel is visible
	virtual void DrawSettings() {};

	// Load settings from ini
	virtual void LoadSettings(CSimpleIni* ini) {};

	// Save settings to ini
	virtual void SaveSettings(CSimpleIni* ini) {};

	virtual bool ToggleVisible() { return visible = !visible; };
	// true if the interface (usually window) is visible, unused if the module has no interface
	bool visible = false;

protected:
	void LoadSettingVisible(CSimpleIni* ini);
	void SaveSettingVisible(CSimpleIni* ini) const;
};
