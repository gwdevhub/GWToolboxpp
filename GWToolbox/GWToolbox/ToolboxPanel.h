#pragma once

#include <d3dx9tex.h>

#include "ToolboxModule.h"

/*
The difference between a ToolboxPanel and a ToolboxModule
is that the Panel also has a button in the main interface
from which it can be opened. 

This class provides the abstraction to group all such panels,
and a function to draw their main window button.
*/

class ToolboxPanel : public ToolboxModule {
public:
	ToolboxPanel() {};
	virtual ~ToolboxPanel() {
		if (texture) texture->Release(); texture = nullptr;
	}

	virtual const char* TabButtonText() const = 0;

	// returns true if clicked
	virtual bool DrawTabButton(IDirect3DDevice9* device) {
		ImGui::PushStyleColor(ImGuiCol_Button, visible ? 
			ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered] : ImVec4(0, 0, 0, 0));
		bool clicked = ImGui::Button(TabButtonText(), ImVec2(ImGui::GetWindowContentRegionWidth(), 0));
		if (clicked) visible = !visible;
		ImGui::PopStyleColor();
		return clicked;
	}

protected:
	IDirect3DTexture9* texture = nullptr;
};
