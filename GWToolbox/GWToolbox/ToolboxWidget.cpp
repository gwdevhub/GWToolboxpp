#include "ToolboxWidget.h"

#include <imgui.h>
#include <imgui_internal.h>
#include "ImGuiAddons.h"

#include "Modules\ToolboxSettings.h"

void ToolboxWidget::DrawSettings() {
	if (ImGui::CollapsingHeader(Name(), ImGuiTreeNodeFlags_AllowItemOverlap)) {
		ImGui::PushID(Name());
		ShowVisibleRadio();
		ImVec2 pos(0, 0);
		if (ImGuiWindow* window = ImGui::FindWindowByName(Name())) pos = window->Pos;
		if (ImGui::DragFloat2("Position", (float*)&pos, 1.0f, 0.0f, 0.0f, "%.0f")) {
			ImGui::SetWindowPos(Name(), pos);
		}
		ImGui::ShowHelp("You need to show the widget for this control to work");
		ImGui::Checkbox("Lock Position", &lock_move);
		ImGui::SameLine();
		ImGui::Checkbox("Lock Size", &lock_size);
		DrawSettingInternal();
		ImGui::PopID();
	} else {
		ShowVisibleRadio();
	}
}

ImGuiWindowFlags ToolboxWidget::GetWinFlags(
	ImGuiWindowFlags flags, bool noinput_if_frozen) const {
	flags |= ImGuiWindowFlags_NoTitleBar;
	flags |= ImGuiWindowFlags_NoScrollbar;
	if (!ToolboxSettings::move_all) {
		if (lock_move) flags |= ImGuiWindowFlags_NoMove;
		if (lock_size) flags |= ImGuiWindowFlags_NoResize;
		if (noinput_if_frozen && lock_move && lock_size) flags |= ImGuiWindowFlags_NoInputs;
	}
	return flags;
}
