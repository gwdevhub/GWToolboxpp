#include "TimerWidget.h"

#include <logger.h>
#include <GWCA\GWCA.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\ChatMgr.h>

#include "GuiUtils.h"
#include "Modules\ToolboxSettings.h"

void TimerWidget::LoadSettings(CSimpleIni *ini) {
	ToolboxWidget::LoadSettings(ini);
	click_to_print_time = ini->GetBoolValue(Name(), VAR_NAME(click_to_print_time), false);
}

void TimerWidget::SaveSettings(CSimpleIni *ini) {
	ToolboxWidget::SaveSettings(ini);
	ini->SetBoolValue(Name(), VAR_NAME(click_to_print_time), click_to_print_time);
}

void TimerWidget::DrawSettingInternal() {
	ToolboxWidget::DrawSettingInternal();
	ImGui::Checkbox("Ctrl+Click to print time", &click_to_print_time);
}

void TimerWidget::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;
	if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;

	unsigned long time = GW::Map::GetInstanceTime() / 1000;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
	ImGui::SetNextWindowSize(ImVec2(250.0f, 90.0f), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !click_to_print_time))) {
		static char timer[32];
		static char urgoz_timer[32];
		snprintf(timer, 32, "%d:%02d:%02d", time / (60 * 60), (time / 60) % 60, time % 60);
		ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f48));
		ImVec2 cur = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
		ImGui::TextColored(ImColor(0, 0, 0), timer);
		ImGui::SetCursorPos(cur);
		ImGui::Text(timer);
		ImGui::PopFont();
		if (GW::Map::GetMapID() == GW::Constants::MapID::Urgozs_Warren
			&& GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
			ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f24));
			ImVec2 cur = ImGui::GetCursorPos();
			int temp = (time - 1) % 25;
			if (temp < 15) {
				snprintf(urgoz_timer, 32, "Open - %d", 15 - temp);
			}  else {
				snprintf(urgoz_timer, 32, "Closed - %d", 25 - temp);
			}

			ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
			ImGui::TextColored(ImColor(0, 0, 0), urgoz_timer);
			ImGui::SetCursorPos(cur);
			ImColor color = temp < 15 ? ImColor(0, 255, 0) : ImColor(255, 0, 0);
			ImGui::TextColored(color, urgoz_timer);
			ImGui::PopFont();
		}
		if (click_to_print_time) {
			ImVec2 size = ImGui::GetWindowSize();
			ImVec2 min = ImGui::GetWindowPos();
			ImVec2 max(min.x + size.x, min.y + size.y);
			bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
			if (ctrl_pressed && ImGui::IsMouseReleased(0) && ImGui::IsMouseHoveringRect(min, max)) {
				GW::Chat::SendChat('/', "age");
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleColor();
}
