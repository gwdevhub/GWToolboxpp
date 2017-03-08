#include "ClockWindow.h"

#include "GuiUtils.h"
#include "OtherModules\ToolboxSettings.h"

void ClockWindow::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;
	
	SYSTEMTIME time;
	GetLocalTime(&time);

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(0, 0, 0, 0));
	ImGui::SetNextWindowSize(ImVec2(250.0f, 90.0f), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), nullptr, GetWinFlags())) {
		static char timer[32];
		if (use_24h_clock) {
			sprintf_s(timer, "%02d:%02d", time.wHour, time.wMinute);
		} else {
			int hour = time.wHour % 12;
			if (hour == 0) hour = 12;
			sprintf_s(timer, "%d:%02d %s", hour, time.wMinute, (time.wHour >= 12 ? "p.m." : "a.m."));
		}
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
	ToolboxWidget::LoadSettings(ini);
	use_24h_clock = ini->GetBoolValue(Name(), "use_24h_clock", true);
}

void ClockWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWidget::LoadSettings(ini);
	ini->SetBoolValue(Name(), "use_24h_clock", use_24h_clock);
}

void ClockWindow::DrawSettingInternal() {
	ImGui::Checkbox("Use 24h clock", &use_24h_clock);
}
