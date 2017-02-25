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
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImVec2 textsize = ImGui::CalcTextSize(TabButtonText());
		float width = ImGui::GetWindowContentRegionWidth();
		float img_size = 24.0f;
		float text_x = pos.x + img_size + (width - img_size - textsize.x) / 2;
		bool clicked = ImGui::Button("", ImVec2(width, textsize.y + ImGui::GetStyle().ItemSpacing.y));
		if (texture != nullptr) {
			ImGui::GetWindowDrawList()->AddImage((ImTextureID)texture, pos, 
				ImVec2(pos.x + img_size, pos.y + img_size));
		}
		ImGui::GetWindowDrawList()->AddText(ImVec2(text_x, pos.y), 
			ImColor(ImGui::GetStyle().Colors[ImGuiCol_Text]), TabButtonText());
		if (clicked) visible = !visible;
		ImGui::PopStyleColor();
		return clicked;
	}

protected:
	IDirect3DTexture9* texture = nullptr;
};
