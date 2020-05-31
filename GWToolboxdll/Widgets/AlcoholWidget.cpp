#include "stdafx.h"
#include "AlcoholWidget.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Title.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <GuiUtils.h>


void AlcoholWidget::Initialize() {
	ToolboxWidget::Initialize();
	// how much time was queued up with drinks
	alcohol_time = 0;
	// last time the player used a drink
	last_alcohol = 0;
	alcohol_level = 0;
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PostProcess>(
        &PostProcess_Entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::PostProcess* pak) {
            UNREFERENCED_PARAMETER(status);
		    AlcUpdate(pak);
	    });
}
uint32_t AlcoholWidget::GetAlcoholTitlePoints() {
	GW::GameContext* gameContext = GW::GameContext::instance();
	if (!gameContext || !gameContext->world || !gameContext->world->titles.valid())
		return 0;	// Sanity checks; context not ready.
	return gameContext->world->titles[7].current_points;
}
uint32_t AlcoholWidget::GetAlcoholTitlePointsGained() {
	uint32_t current_title_points = GetAlcoholTitlePoints();
	uint32_t points_gained = current_title_points - prev_alcohol_title_points;
	prev_alcohol_title_points = current_title_points; // Update previous variable.
	return points_gained <= 0 ? 0 : points_gained;
}
void AlcoholWidget::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
	if (map_id != GW::Map::GetMapID() && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading) {
		map_id = GW::Map::GetMapID();
		prev_alcohol_title_points = GetAlcoholTitlePoints(); // Fetch base alcohol points at start of map.
	}
}
uint32_t AlcoholWidget::GetAlcoholLevel() {
	return alcohol_level;
}
void AlcoholWidget::AlcUpdate(GW::Packet::StoC::PostProcess *packet) {
	uint32_t pts_gained = GetAlcoholTitlePointsGained();
	//Log::Info("Drunk effect %d / %d, %d pts gained", packet->tint, packet->level, pts_gained);
	if (packet->tint == 6) {
		// Tint 6, level 5 - the trouble zone for lunars!
		if (packet->level == 5 && (prev_packet_tint_6_level < packet->level-1 || (prev_packet_tint_6_level == 5 && pts_gained < 1))) {
			// If we've jumped a level, or the last packet was also level 5 and no points were gained, then its not alcohol.
			// NOTE: All alcohol progresses from 1 to 5, but lunars just dive in at level 5.
			//Log::Info("Lunars detected");
			return;
		}
		prev_packet_tint_6_level = packet->level;
	}
	// if the player used a drink
	if (packet->level > alcohol_level){
		// if the player already had a drink going
		if (alcohol_level) {
			// set remaining time
			alcohol_time = (int)((long)alcohol_time + (long)last_alcohol - (long)time(NULL));
		}
		// add drink time
		alcohol_time += 60 * (int)(packet->level - alcohol_level);
		last_alcohol = time(NULL);
	} else if (packet->level <= alcohol_level) {
		alcohol_time = 60 * (int)packet->level;
		last_alcohol = time(NULL);
	}
	alcohol_level = packet->level;
}

void AlcoholWidget::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
	if (!visible) return;

	if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) return;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
	ImGui::SetNextWindowSize(ImVec2(200.0f, 90.0f), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, true))) {
		if (alcohol_level || !only_show_when_drunk) {
			static char timer[32];
			ImVec2 cur;
			long t = 0;

			if (alcohol_level) {
				t = (long)((int)last_alcohol + ((int)alcohol_time)) - (long)time(NULL);
			}
			else if (only_show_when_drunk) {
				return;
			}
			snprintf(timer, 32, "%1ld:%02ld", (t / 60) % 60, t % 60);

			ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f20));
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
			ImGui::TextColored(ImColor(0, 0, 0), "Alcohol");
			ImGui::SetCursorPos(cur);
			ImGui::Text("Alcohol");
			ImGui::PopFont();

			ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f48));
			cur = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
			ImGui::TextColored(ImColor(0, 0, 0), timer);
			ImGui::SetCursorPos(cur);
			ImGui::Text(timer);
			ImGui::PopFont();
		}
	}
	ImGui::End();
	ImGui::PopStyleColor();
}

void AlcoholWidget::DrawSettingInternal() {
	ImGui::Checkbox("Only show when drunk", &only_show_when_drunk);
	ImGui::ShowHelp("Hides widget when not using alcohol");
	ImGui::Text("Note: only visible in explorable areas.");
}
void AlcoholWidget::LoadSettings(CSimpleIni* ini) {
	ToolboxWidget::LoadSettings(ini);
	only_show_when_drunk = ini->GetBoolValue(Name(), VAR_NAME(only_show_when_drunk), only_show_when_drunk);
}

void AlcoholWidget::SaveSettings(CSimpleIni* ini) {
	ToolboxWidget::SaveSettings(ini);
	ini->SetBoolValue(Name(), VAR_NAME(only_show_when_drunk), only_show_when_drunk);
}
