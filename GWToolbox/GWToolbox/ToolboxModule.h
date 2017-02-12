#pragma once

#include <imgui.h>
#include <DirectX\Include\d3d9.h>
#include <SimpleIni.h>

class ToolboxModule {
public:
	// name of the window and the ini section
	virtual const char* Name() = 0;

	// Update. Will always be called once every frame.
	virtual void Update() {};

	// Draw user interface. Will be called every frame if the element is visible
	virtual void Draw(IDirect3DDevice9* pDevice) {};

	// Draw settings interface. Will be called if the setting panel is visible
	virtual void DrawSettings() {};

	// Load settings from ini
	virtual void LoadSettings(CSimpleIni* ini) {};

	// Save settings to ini
	virtual void SaveSettings(CSimpleIni* ini) {};
};
