#pragma once

#include <imgui.h>
#include <d3d9.h>
#include <SimpleIni.h>

class ToolboxModule {
public:
	ToolboxModule() {}
	virtual ~ToolboxModule() {};

public:
	// name of the window and the ini section
	virtual const char* Name() const = 0;

	// Initialize module
	virtual void Initialize();

	// Terminate module
	virtual void Terminate() {};

	// Update. Will always be called once every frame. Delta in seconds
	virtual void Update(float delta) {};

	// This is provided (and called), but use ImGui::GetIO() during update/render if possible.
	virtual bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) { return false; };

	// Load what is needed from ini
	virtual void LoadSettings(CSimpleIni* ini) {};

	// Save what is needed to ini
	virtual void SaveSettings(CSimpleIni* ini) {};

	// Draw settings interface. Will be called if the setting panel is visible, calls DrawSettingsInternal()
	virtual void DrawSettings();
	virtual void DrawSettingInternal() {};
};
