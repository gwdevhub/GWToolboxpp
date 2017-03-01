#pragma once

#include "ToolboxModule.h"

/*
A ToolboxWindow is a module which also has an interface
*/
class ToolboxWindow : public ToolboxModule {
public:
	// save 'visible' field
	virtual void LoadSettings(CSimpleIni* ini) override;

	// load 'visible' field
	virtual void SaveSettings(CSimpleIni* ini) override;

	// Encapsulate settings into an ImGui::CollapsingHeader(Name()), 
	// show a reset position button and a 'visible' checkbox
	virtual void DrawSettings() override;

	virtual bool ToggleVisible() { return visible = !visible; };

	// true if the interface is visible
	bool visible = false;

protected:
	void ShowVisibleRadio();
};
