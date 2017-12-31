#include "ToolboxUIElement.h"

#include <GWToolbox.h>

void ToolboxUIElement::Initialize() {
	ToolboxModule::Initialize();
	GWToolbox::Instance().RegisterUIElement(this);
}

void ToolboxUIElement::LoadSettings(CSimpleIni* ini) {
	ToolboxModule::LoadSettings(ini);
	visible = ini->GetBoolValue(Name(), VAR_NAME(visible), false);
	show_menubutton = ini->GetBoolValue(Name(), VAR_NAME(show_menubutton), false);
}

void ToolboxUIElement::SaveSettings(CSimpleIni* ini) {
	ToolboxModule::SaveSettings(ini);
	ini->SetBoolValue(Name(), VAR_NAME(visible), visible);
	ini->SetBoolValue(Name(), VAR_NAME(show_menubutton), show_menubutton);
}

void ToolboxUIElement::ShowVisibleRadio() {
	ImGui::SameLine(ImGui::GetContentRegionAvailWidth()
		- ImGui::GetTextLineHeight()
		- ImGui::GetStyle().FramePadding.y * 2);
	ImGui::PushID(Name());
	ImGui::Checkbox("##check", &visible);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Visible");
	ImGui::PopID();
}

bool ToolboxUIElement::DrawTabButton(IDirect3DDevice9* device, 
	bool show_icon, bool show_text) {

	ImGui::PushStyleColor(ImGuiCol_Button, visible ?
		ImGui::GetStyle().Colors[ImGuiCol_Button] : ImVec4(0, 0, 0, 0));
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImVec2 textsize = ImGui::CalcTextSize(Name());
	float width = ImGui::GetWindowContentRegionWidth();
	
	float img_size = 0;
	if (show_icon && button_texture != nullptr) {
		img_size = ImGui::GetTextLineHeightWithSpacing();
	}
	float text_x = pos.x + img_size + (width - img_size - textsize.x) / 2;
	bool clicked = ImGui::Button("", ImVec2(width, ImGui::GetTextLineHeightWithSpacing()));
	if (show_icon && button_texture != nullptr) {
		ImGui::GetWindowDrawList()->AddImage((ImTextureID)button_texture, pos,
			ImVec2(pos.x + img_size, pos.y + img_size));
	}
	if (show_text) {
		ImGui::GetWindowDrawList()->AddText(ImVec2(text_x, pos.y + ImGui::GetStyle().ItemSpacing.y / 2),
			ImColor(ImGui::GetStyle().Colors[ImGuiCol_Text]), Name());
	}
	
	if (clicked) visible = !visible;
	ImGui::PopStyleColor();
	return clicked;
}
