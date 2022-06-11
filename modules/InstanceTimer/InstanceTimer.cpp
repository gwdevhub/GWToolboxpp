#include "InstanceTimer.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameContainers/Array.h>

#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>

#include <logger.h>
#include <Timer.h>

#include "GuiUtils.h"

DLLAPI TBModule* TBModuleInstance() {
    static InstanceTimer instance;
    return &instance;
}

void InstanceTimer::LoadSettings(CSimpleIni *ini) {
	TBModule::LoadSettings(ini);
	click_to_print_time = ini->GetBoolValue(Name(), VAR_NAME(click_to_print_time), false);
    show_extra_timers = ini->GetBoolValue(Name(), VAR_NAME(show_extra_timers), false);
}

void InstanceTimer::SaveSettings(CSimpleIni *ini) {
    TBModule::SaveSettings(ini);
	ini->SetBoolValue(Name(), VAR_NAME(click_to_print_time), click_to_print_time);
    ini->SetBoolValue(Name(), VAR_NAME(show_extra_timers), show_extra_timers);
}

void InstanceTimer::DrawSettings() {
	ImGui::Checkbox("Ctrl+Click to print time", &click_to_print_time);
    ImGui::Checkbox("Show extra timers", &show_extra_timers);
    ImGui::ShowHelp("Such as Deep aspects");
}

void InstanceTimer::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;
	if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;

	unsigned long time = GW::Map::GetInstanceTime() / 1000;

    bool ctrl_pressed = ImGui::IsKeyDown(VK_CONTROL);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
	ImGui::SetNextWindowSize(ImVec2(250.0f, 90.0f), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !(click_to_print_time && ctrl_pressed)))) {
		snprintf(timer_buffer, 32, "%d:%02d:%02d", time / (60 * 60), (time / 60) % 60, time % 60);
		ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f48));
		ImVec2 cur = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
		ImGui::TextColored(ImColor(0, 0, 0), timer_buffer);
		ImGui::SetCursorPos(cur);
		ImGui::Text(timer_buffer);
		ImGui::PopFont();

        if (GetUrgozTimer() || (show_extra_timers && (GetDeepTimer() || GetDhuumTimer() || GetTrapTimer()))) {

            ImGui::PushFont(GuiUtils::GetFont(GuiUtils::f24));
            ImVec2 cur2 = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(cur2.x + 2, cur2.y + 2));
            ImGui::TextColored(ImColor(0, 0, 0), extra_buffer);
            ImGui::SetCursorPos(cur2);
            ImGui::TextColored(extra_color, extra_buffer);
            ImGui::PopFont();
        }

		if (click_to_print_time) {
			ImVec2 size = ImGui::GetWindowSize();
			ImVec2 min = ImGui::GetWindowPos();
			ImVec2 max(min.x + size.x, min.y + size.y);
			if (ctrl_pressed && ImGui::IsMouseReleased(0) && ImGui::IsMouseHoveringRect(min, max)) {
				GW::Chat::SendChat('/', "age");
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleColor();
}

bool InstanceTimer::GetUrgozTimer() {
    if (GW::Map::GetMapID() != GW::Constants::MapID::Urgozs_Warren) return false;
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) return false;
    unsigned long time = GW::Map::GetInstanceTime() / 1000;
    int temp = (time - 1) % 25;
    if (temp < 15) {
        snprintf(extra_buffer, 32, "Open - %d", 15 - temp);
        extra_color = ImColor(0, 255, 0);
    } else {
        snprintf(extra_buffer, 32, "Closed - %d", 25 - temp);
        extra_color = ImColor(255, 0, 0);
    }
    return true;
}

bool InstanceTimer::GetDeepTimer() {
    using namespace GW::Constants;

    if (GW::Map::GetMapID() != MapID::The_Deep) return false;
    if (GW::Map::GetInstanceType() != InstanceType::Explorable) return false;
    
    GW::EffectArray effects = GW::Effects::GetPlayerEffectArray();
    if (!effects.valid()) return false;

    static clock_t start = -1;
    SkillID skill = SkillID::No_Skill;
    for (DWORD i = 0; i < effects.size(); ++i) {
        SkillID effect_id = (SkillID)effects[i].skill_id;
        switch (effect_id) {
        case SkillID::Aspect_of_Exhaustion: skill = SkillID::Aspect_of_Exhaustion; break;
        case SkillID::Aspect_of_Depletion_energy_loss: skill = SkillID::Aspect_of_Depletion_energy_loss; break;
        case SkillID::Scorpion_Aspect: skill = SkillID::Scorpion_Aspect; break;
        default: break;
        }
    }
    if (skill == SkillID::No_Skill) {
        start = -1;
        return false;
    }

    if (start == -1) {
        start = TIMER_INIT();
    }

    clock_t diff = TIMER_DIFF(start) / 1000;


    // a 30s timer starts when you enter the aspect
    // a 30s timer starts 100s after you enter the aspect
    // a 30s timer starts 200s after you enter the aspect
    long timer = 30 - (diff % 30);
    if (diff > 100) timer = std::min(timer, 30 - ((diff - 100) % 30));
    if (diff > 200) timer = std::min(timer, 30 - ((diff - 200) % 30));
    switch (skill) {
    case SkillID::Aspect_of_Exhaustion: 
        snprintf(extra_buffer, 32, "Exhaustion: %d", timer);
        break;
    case SkillID::Aspect_of_Depletion_energy_loss: 
        snprintf(extra_buffer, 32, "Depletion: %d", timer);
        break;
    case SkillID::Scorpion_Aspect: 
        snprintf(extra_buffer, 32, "Scorpion: %d", timer);
        break;
    default:
        break;
    }
    extra_color = ImColor(255, 255, 255);
    return true;
}

bool InstanceTimer::GetDhuumTimer() {
    // todo: implement
    return false;
}

bool InstanceTimer::GetTrapTimer() {
    using namespace GW::Constants;
    if (GW::Map::GetInstanceType() != InstanceType::Explorable) return false;

    unsigned long time = GW::Map::GetInstanceTime() / 1000;
    int temp = time % 20;
    int timer;
    if (temp < 10) {
        timer = 10 - temp;
        extra_color = ImColor(0, 255, 0);
    } else {
        timer = 20 - temp;
        extra_color = ImColor(255, 0, 0);
    }

    switch (GW::Map::GetMapID()) {
    case MapID::Catacombs_of_Kathandrax_Level_1:
    case MapID::Catacombs_of_Kathandrax_Level_2:
    case MapID::Catacombs_of_Kathandrax_Level_3:
    case MapID::Bloodstone_Caves_Level_1:
    case MapID::Arachnis_Haunt_Level_2:
    case MapID::Oolas_Lab_Level_2:
        snprintf(extra_buffer, 32, "Fire Jet: %d", timer);
        return true;
    case MapID::Heart_of_the_Shiverpeaks_Level_3:
        snprintf(extra_buffer, 32, "Fire Spout: %d", timer);
        return true;
    case MapID::Shards_of_Orr_Level_3:
    case MapID::Cathedral_of_Flames_Level_3:
        snprintf(extra_buffer, 32, "Fire Trap: %d", timer);
        return true;
    case MapID::Sepulchre_of_Dragrimmar_Level_1:
    case MapID::Ravens_Point_Level_1:
    case MapID::Ravens_Point_Level_2:
    case MapID::Heart_of_the_Shiverpeaks_Level_1:
    case MapID::Darkrime_Delves_Level_2:
        snprintf(extra_buffer, 32, "Ice Jet: %d", timer);
        return true;
    case MapID::Darkrime_Delves_Level_1:
    case MapID::Secret_Lair_of_the_Snowmen:
        snprintf(extra_buffer, 32, "Ice Spout: %d", timer);
        return true;
    case MapID::Bogroot_Growths_Level_1:
    case MapID::Arachnis_Haunt_Level_1:
    case MapID::Shards_of_Orr_Level_1:
    case MapID::Shards_of_Orr_Level_2:
        snprintf(extra_buffer, 32, "Poison Jet: %d", timer);
        return true;
    case MapID::Bloodstone_Caves_Level_2:
        snprintf(extra_buffer, 32, "Poison Spout: %d", timer);
        return true;
    case MapID::Cathedral_of_Flames_Level_2:
    case MapID::Bloodstone_Caves_Level_3:
        snprintf(extra_buffer, 32, "Poison Trap: %d", timer);
        return true;
    default:
        return false;
    }
}
