#include "NotePadWindow.h"

#include <fstream>

#include <Modules\Resources.h>

#define TEXT_SIZE 2024 * 16

void NotePadWindow::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(300.0f, 200.0f), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
		ImVec2 cmax = ImGui::GetWindowContentRegionMax();
		ImVec2 cmin = ImGui::GetWindowContentRegionMin();
		if (ImGui::InputTextMultiline("##source", text, TEXT_SIZE,
			ImVec2(cmax.x - cmin.x, cmax.y - cmin.y), ImGuiInputTextFlags_AllowTabInput)) {
			filedirty = true;
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

void NotePadWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWindow::LoadSettings(ini);

	std::ifstream file(Resources::GetPath("Notepad.txt"));
	if (file) {
		file.get(text, TEXT_SIZE, '\0');
		file.close();
	}
}

void NotePadWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);

	if (filedirty) {
		std::ofstream file(Resources::GetPath("Notepad.txt"));
		if (file) {
			file.write(text, strlen(text));
			file.close();
			filedirty = false;
		}
	}
}
