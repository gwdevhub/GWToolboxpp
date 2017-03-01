#include "ImGuiAddons.h"

#include <imgui.h>
#include <imgui_internal.h>

void ImGui::ShowHelp(const char* help) {
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(help);
	}
}
