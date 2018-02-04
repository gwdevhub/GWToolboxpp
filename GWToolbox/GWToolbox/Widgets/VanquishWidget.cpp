#include "VanquishWidget.h"

#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\PartyMgr.h>

#include "GuiUtils.h"

void VanquishWidget::Draw(IDirect3DDevice9 *pDevice) {
	if (!visible) return;

	DWORD tokill = GW::Map::GetFoesToKill();
	DWORD killed = GW::Map::GetFoesKilled();

	if ((GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) ||
		!GW::PartyMgr::GetIsPartyInHardMode() ||
		tokill <= 0) return;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
	ImGui::SetNextWindowSize(ImVec2(250.0f, 90.0f), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), nullptr, GetWinFlags())) {
		static char foes_count[32] = "";
		snprintf(foes_count, 32, "%u / %u", killed, tokill + killed);

		// vanquished
		ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f20));
		ImVec2 cur = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
		ImGui::TextColored(ImColor(0, 0, 0), "Vanquished");
		ImGui::SetCursorPos(cur);
		ImGui::Text("Vanquished");
		ImGui::PopFont();

		// count
		ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f42));
		cur = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
		ImGui::TextColored(ImColor(0, 0, 0), foes_count);
		ImGui::SetCursorPos(cur);
		ImGui::Text(foes_count);
		ImGui::PopFont();
	}
	ImGui::End();
	ImGui::PopStyleColor();
}

void VanquishWidget::DrawSettingInternal() {
	ImGui::Text("Note: only visible in Hard Mode explorable areas.");
}
