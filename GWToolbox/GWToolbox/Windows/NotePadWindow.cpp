#include "NotePadWindow.h"

void NotePadWindow::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(300.0f, 200.0f), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), &visible)) {
		ImVec2 cmax = ImGui::GetWindowContentRegionMax();
		ImVec2 cmin = ImGui::GetWindowContentRegionMin();
		ImGui::InputTextMultiline("##source", text, 2024 * 16,
			ImVec2(cmax.x - cmin.x, cmax.y - cmin.y), ImGuiInputTextFlags_AllowTabInput);
	}
	ImGui::End();
	ImGui::PopStyleVar();
}
