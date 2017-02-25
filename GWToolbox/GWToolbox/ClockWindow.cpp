#include "ClockWindow.h"

#include "GuiUtils.h"
#include "GWToolbox.h"

void ClockWindow::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;

	SYSTEMTIME time;
	GetLocalTime(&time);

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(0, 0, 0, 0));
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar;
	if (GWToolbox::instance().WidgetsFreezed()) {
		flags |= ImGuiWindowFlags_NoInputs;
	}
	ImGui::SetNextWindowSize(ImVec2(250.0f, 90.0f));
	if (ImGui::Begin(Name(), nullptr, flags)) {
		static char timer[32];
		sprintf_s(timer, "%02d:%02d", time.wHour, time.wMinute);
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[GuiUtils::FontSize::f30]);
		ImVec2 cur = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
		ImGui::TextColored(ImColor(0, 0, 0), timer);
		ImGui::SetCursorPos(cur);
		ImGui::Text(timer);
		ImGui::PopFont();
	}
	ImGui::End();
	ImGui::PopStyleColor();
}

void ClockWindow::LoadSettings(CSimpleIni* ini) {
	LoadSettingVisible(ini);
}

void ClockWindow::SaveSettings(CSimpleIni* ini) {
	SaveSettingVisible(ini);
}
