#include "ToolboxPanel.h"

void ToolboxPanel::DrawSettings() {
	if (ImGui::TreeNode(Name())) {
		DrawSettingInternal();
		ImGui::TreePop();
	}
}

bool ToolboxPanel::DrawTabButton(IDirect3DDevice9* device) {
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

