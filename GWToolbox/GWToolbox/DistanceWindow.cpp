#include "DistanceWindow.h"

#include <GWCA\GWCA.h>

#include "GuiUtils.h"
#include "GWToolbox.h"
#include "OtherSettings.h"

void DistanceWindow::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(0, 0, 0, 0));
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar;
	if (GWToolbox::instance().other_settings->freeze_widgets) {
		flags |= ImGuiWindowFlags_NoInputs;
	}
	ImGui::SetNextWindowSize(ImVec2(150, 100));
	if (ImGui::Begin(Name(), nullptr, flags)) {
		static char dist_perc[32];
		static char dist_abs[32];
		GW::Agent* me = GW::Agents().GetPlayer();
		GW::Agent* target = GW::Agents().GetTarget();
		if (me && target && me != target) {
			float dist = GW::Agents().GetDistance(me->pos, target->pos);
			sprintf_s(dist_perc, "%2.0f %s", dist * 100 / GW::Constants::Range::Compass, "%%");
			sprintf_s(dist_abs, "%.0f", dist);

			ImVec2 cur;

			// 'distance'
			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[GuiUtils::FontSize::f12]);
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
			ImGui::TextColored(ImColor(0, 0, 0), "Distance");
			ImGui::SetCursorPos(cur);
			ImGui::Text("Distance");
			ImGui::PopFont();

			// perc
			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[GuiUtils::FontSize::f26]);
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
			ImGui::TextColored(ImColor(0, 0, 0), dist_perc);
			ImGui::SetCursorPos(cur);
			ImGui::Text(dist_perc);
			ImGui::PopFont();

			// abs
			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[GuiUtils::FontSize::f16]);
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
			ImGui::TextColored(ImColor(0, 0, 0), dist_abs);
			ImGui::SetCursorPos(cur);
			ImGui::Text(dist_abs);
			ImGui::PopFont();
		}
	}
	ImGui::End();
	ImGui::PopStyleColor();
}


void DistanceWindow::LoadSettings(CSimpleIni* ini) {
	LoadSettingVisible(ini);
}

void DistanceWindow::SaveSettings(CSimpleIni* ini) const {
	SaveSettingVisible(ini);
}
