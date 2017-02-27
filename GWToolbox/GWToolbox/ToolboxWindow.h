#pragma once

#include "ToolboxModule.h"

/*
A ToolboxWindow is a module which also has an interface
*/
class ToolboxWindow : public ToolboxModule {
public:
	ToolboxWindow() {};
	virtual ~ToolboxWindow() {};

	// Draw settings interface. Will be called if the setting panel is visible
	// By default, encapsulate settings into an ImGui::CollapsingHeader(Name()), 
	// show a reset position button and a 'visible' checkbox
	virtual void DrawSettings() override;

	// Load settings from ini
	// By default, save 'visible' field
	virtual void LoadSettings(CSimpleIni* ini) override;

	// Save settings to ini
	// By default, load 'visible' field
	virtual void SaveSettings(CSimpleIni* ini) override;

	virtual bool ToggleVisible() { return visible = !visible; };

	// true if the interface is visible
	bool visible = false;
};
