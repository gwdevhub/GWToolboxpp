#include "NotePadWindow.h"

#include "GuiUtils.h"
#include "GWToolbox.h"

NotePadWindow::NotePadWindow() {
	strcpy_s(text, "");
}

void NotePadWindow::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGuiWindowFlags flags = 0;
	//if (GWToolbox::instance().WidgetsFreezed()) { // maybe not?
	//	flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoResize;
	//}
	ImGui::SetNextWindowSize(ImVec2(300.0f, 200.0f), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), nullptr, flags)) {
		ImVec2 cmax = ImGui::GetWindowContentRegionMax();
		ImVec2 cmin = ImGui::GetWindowContentRegionMin();
		ImGui::InputTextMultiline("##source", text, 2024 * 16,
			ImVec2(cmax.x - cmin.x, cmax.y - cmin.y), ImGuiInputTextFlags_AllowTabInput);
	}
	ImGui::End();
	ImGui::PopStyleVar();
}
