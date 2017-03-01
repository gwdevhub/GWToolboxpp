#pragma once

#include <d3dx9tex.h>

#include "ToolboxWindow.h"

/*
The difference between a ToolboxPanel and a ToolboxWindow
is that the Panel also has a button in the main interface
from which it can be opened. 

This class provides the abstraction to group all such panels,
and a function to draw their main window button.
*/

class ToolboxPanel : public ToolboxWindow {
public:
	// Terminate module, save what is needed to ini
	virtual void Terminate() {
		ToolboxWindow::Terminate();
		if (texture) texture->Release();
	}

	// Draw settings interface. Will be called if the setting panel is visible
	// By default, encapsulate settings into an ImGui::CollapsingHeader(Name()), 
	// show a reset position button and a 'visible' checkbox
	virtual void DrawSettings() override;

	// returns true if clicked
	virtual bool DrawTabButton(IDirect3DDevice9* device);

protected:
	IDirect3DTexture9* texture = nullptr;
};
