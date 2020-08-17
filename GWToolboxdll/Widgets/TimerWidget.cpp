#include "stdafx.h"

#include <GWCA/GameContainers/Array.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <GuiUtils.h>
#include <Logger.h>
#include <Timer.h>

#include <Modules/ToolboxSettings.h>
#include <Widgets/TimerWidget.h>

void TimerWidget::Initialize() {
    ToolboxWidget::Initialize();
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayDialogue>(&DisplayDialogue_Entry,
        [this](GW::HookStatus*, GW::Packet::StoC::DisplayDialogue* packet) -> void {
            DisplayDialogue(packet);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(&GameSrvTransfer_Entry,
        [this](GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer*) -> void {
            cave_start = 0;
        });
}

void TimerWidget::LoadSettings(CSimpleIni *ini) {
    ToolboxWidget::LoadSettings(ini);
    click_to_print_time = ini->GetBoolValue(Name(), VAR_NAME(click_to_print_time), click_to_print_time);
    show_extra_timers = ini->GetBoolValue(Name(), VAR_NAME(show_extra_timers), show_extra_timers);
    hide_in_outpost = ini->GetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);
    show_spirit_timers = ini->GetBoolValue(Name(), VAR_NAME(show_spirit_timers), show_spirit_timers);
    for (auto it : spirit_effects) {
        char ini_name[32];
        snprintf(ini_name, 32, "spirit_effect_%d", it.first);
        *spirit_effects_enabled[it.first] = ini->GetBoolValue(Name(), ini_name, *spirit_effects_enabled[it.first]);
    }
}

void TimerWidget::SaveSettings(CSimpleIni *ini) {
    ToolboxWidget::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(click_to_print_time), click_to_print_time);
    ini->SetBoolValue(Name(), VAR_NAME(show_extra_timers), show_extra_timers);
    ini->SetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);
    ini->SetBoolValue(Name(), VAR_NAME(show_spirit_timers), show_spirit_timers);
    for (auto it : spirit_effects) {
        char ini_name[32];
        snprintf(ini_name, 32, "spirit_effect_%d", it.first);
        ini->SetBoolValue(Name(), ini_name, *spirit_effects_enabled[it.first]);
    }
}

void TimerWidget::DrawSettingInternal() {
    ToolboxWidget::DrawSettingInternal();
    ImGui::SameLine(); ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
    ImGui::Checkbox("Ctrl+Click to print time", &click_to_print_time);
    ImGui::Checkbox("Show extra timers", &show_extra_timers);
    ImGui::ShowHelp("Such as Deep aspects");
    ImGui::Checkbox("Show spirit timers", &show_spirit_timers);
    ImGui::ShowHelp("Time until spirits die in seconds");
    if (show_spirit_timers) {
        ImGui::Indent();
        size_t i = 0;
        for (auto it : spirit_effects) {
            if (i % 3 == 0)
                i = 0;
            else 
                ImGui::SameLine(200.0f * ImGui::GetIO().FontGlobalScale * i);
            i++;
            ImGui::Checkbox(it.second, spirit_effects_enabled[it.first]);
        }
        ImGui::Unindent();
    }
}

ImGuiWindowFlags TimerWidget::GetWinFlags(ImGuiWindowFlags flags, bool noinput_if_frozen) const {
    return ToolboxWidget::GetWinFlags(flags, noinput_if_frozen) | (lock_size ? ImGuiWindowFlags_AlwaysAutoResize : 0);
}

void TimerWidget::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;
    if (hide_in_outpost && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
        return;
    unsigned long time = GW::Map::GetInstanceTime() / 1000;

    bool ctrl_pressed = ImGui::IsKeyDown(VK_CONTROL);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(250.0f, 90.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !(click_to_print_time && ctrl_pressed)))) {
        snprintf(timer_buffer, 32, "%lu:%02lu:%02lu", time / (60 * 60), (time / 60) % 60, time % 60);
        ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::f48));
        ImVec2 cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
        ImGui::TextColored(ImColor(0, 0, 0), timer_buffer);
        ImGui::SetCursorPos(cur);
        ImGui::Text(timer_buffer);
        ImGui::PopFont();

        if (show_extra_timers && (GetUrgozTimer() || GetDeepTimer() || GetDhuumTimer() || GetTrapTimer() || GetDoATimer())) {

            ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::f24));
            ImVec2 cur2 = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(cur2.x + 2, cur2.y + 2));
            ImGui::TextColored(ImColor(0, 0, 0), extra_buffer);
            ImGui::SetCursorPos(cur2);
            ImGui::TextColored(extra_color, extra_buffer);
            ImGui::PopFont();
        }
        if (GetSpiritTimer()) {
            ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::f24));
            ImVec2 cur2 = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(cur2.x + 1, cur2.y + 1));
            ImGui::TextColored(ImColor(0, 0, 0), spirits_buffer);
            ImGui::SetCursorPos(cur2);
            ImGui::Text(spirits_buffer);
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

bool TimerWidget::GetUrgozTimer() {
    if (GW::Map::GetMapID() != GW::Constants::MapID::Urgozs_Warren) return false;
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) return false;
    unsigned long time = GW::Map::GetInstanceTime() / 1000;
    unsigned long  temp = (time - 1) % 25;
    if (temp < 15) {
        snprintf(extra_buffer, 32, "Open - %lu", 15u - temp);
        extra_color = ImColor(0, 255, 0);
    } else {
        snprintf(extra_buffer, 32, "Closed - %lu", 25u - temp);
        extra_color = ImColor(255, 0, 0);
    }
    return true;
}

bool TimerWidget::GetSpiritTimer() {
    using namespace GW::Constants;

    if (!show_spirit_timers || GW::Map::GetInstanceType() != InstanceType::Explorable) return false;

    GW::EffectArray effects = GW::Effects::GetPlayerEffectArray();
    if (!effects.valid()) return false;

    int offset = 0;
    for (DWORD i = 0; i < effects.size(); ++i) {
        if (!effects[i].duration)
            continue;
        SkillID effect_id = (SkillID)effects[i].skill_id;
        auto spirit_effect_enabled = spirit_effects_enabled.find(effect_id);
        if (spirit_effect_enabled == spirit_effects_enabled.end() || !*(spirit_effect_enabled->second))
            continue;
        offset += snprintf(&spirits_buffer[offset], sizeof(spirits_buffer) - offset, "%s%s: %d", offset ? "\n" : "", spirit_effects[effect_id], effects[i].GetTimeRemaining() / 1000);
    }
    if (!offset) 
        return false;
    spirits_buffer[offset] = 0;
    return true;
}

bool TimerWidget::GetDeepTimer() {
    using namespace GW::Constants;

    if (GW::Map::GetMapID() != MapID::The_Deep) return false;
    if (GW::Map::GetInstanceType() != InstanceType::Explorable) return false;
    
    GW::EffectArray effects = GW::Effects::GetPlayerEffectArray();
    if (!effects.valid()) return false;

    static clock_t start = -1;
    SkillID skill = SkillID::No_Skill;
    for (DWORD i = 0; i < effects.size() && skill == SkillID::No_Skill; ++i) {
        SkillID effect_id = (SkillID)effects[i].skill_id;
        switch (effect_id) {
        case SkillID::Aspect_of_Exhaustion:
        case SkillID::Aspect_of_Depletion_energy_loss:
        case SkillID::Scorpion_Aspect: 
            skill = effect_id; 
            break;
        default:
            break;
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
        snprintf(extra_buffer, 32, "Exhaustion: %lu", timer);
        break;
    case SkillID::Aspect_of_Depletion_energy_loss: 
        snprintf(extra_buffer, 32, "Depletion: %lu", timer);
        break;
    case SkillID::Scorpion_Aspect: 
        snprintf(extra_buffer, 32, "Scorpion: %lu", timer);
        break;
    default:
        break;
    }
    extra_color = ImColor(255, 255, 255);
    return true;
}

bool TimerWidget::GetDhuumTimer() {
    // todo: implement
    return false;
}

bool TimerWidget::GetTrapTimer() {
    using namespace GW::Constants;
    if (GW::Map::GetInstanceType() != InstanceType::Explorable) return false;

    unsigned long time = GW::Map::GetInstanceTime() / 1000;
    unsigned long temp = time % 20;
    unsigned long timer;
    if (temp < 10) {
        timer = 10u - temp;
        extra_color = ImColor(0, 255, 0);
    } else {
        timer = 20u - temp;
        extra_color = ImColor(255, 0, 0);
    }

    switch (GW::Map::GetMapID()) {
    case MapID::Catacombs_of_Kathandrax_Level_1:
    case MapID::Catacombs_of_Kathandrax_Level_2:
    case MapID::Catacombs_of_Kathandrax_Level_3:
    case MapID::Bloodstone_Caves_Level_1:
    case MapID::Arachnis_Haunt_Level_2:
    case MapID::Oolas_Lab_Level_2:
        snprintf(extra_buffer, 32, "Fire Jet: %lu", timer);
        return true;
    case MapID::Heart_of_the_Shiverpeaks_Level_3:
        snprintf(extra_buffer, 32, "Fire Spout: %lu", timer);
        return true;
    case MapID::Shards_of_Orr_Level_3:
    case MapID::Cathedral_of_Flames_Level_3:
        snprintf(extra_buffer, 32, "Fire Trap: %lu", timer);
        return true;
    case MapID::Sepulchre_of_Dragrimmar_Level_1:
    case MapID::Ravens_Point_Level_1:
    case MapID::Ravens_Point_Level_2:
    case MapID::Heart_of_the_Shiverpeaks_Level_1:
    case MapID::Darkrime_Delves_Level_2:
        snprintf(extra_buffer, 32, "Ice Jet: %lu", timer);
        return true;
    case MapID::Darkrime_Delves_Level_1:
    case MapID::Secret_Lair_of_the_Snowmen:
        snprintf(extra_buffer, 32, "Ice Spout: %lu", timer);
        return true;
    case MapID::Bogroot_Growths_Level_1:
    case MapID::Arachnis_Haunt_Level_1:
    case MapID::Shards_of_Orr_Level_1:
    case MapID::Shards_of_Orr_Level_2:
        snprintf(extra_buffer, 32, "Poison Jet: %lu", timer);
        return true;
    case MapID::Bloodstone_Caves_Level_2:
        snprintf(extra_buffer, 32, "Poison Spout: %lu", timer);
        return true;
    case MapID::Cathedral_of_Flames_Level_2:
    case MapID::Bloodstone_Caves_Level_3:
        snprintf(extra_buffer, 32, "Poison Trap: %lu", timer);
        return true;
    default:
        return false;
    }
}

bool TimerWidget::GetDoATimer() {
    using namespace GW::Constants;

    if (GW::Map::GetInstanceType() != InstanceType::Explorable) return false;
    if (GW::Map::GetMapID() != MapID::Domain_of_Anguish) return false;
    if (cave_start == 0) return false;

    const uint32_t time = GW::Map::GetInstanceTime();

    uint32_t currentWave = 0;
    uint32_t time_since_previous_wave = (time - cave_start) / 1000;
    for (size_t i = 0; i < _countof(CAVE_SPAWN_INTERVALS); i++) {
        if (time_since_previous_wave < CAVE_SPAWN_INTERVALS[i])
            break;
        time_since_previous_wave -= CAVE_SPAWN_INTERVALS[i];
        ++currentWave;
    }
    
    if (currentWave >= _countof(CAVE_SPAWN_INTERVALS))
        return false;

    uint32_t timer = 0;
    if (time_since_previous_wave < CAVE_SPAWN_INTERVALS[currentWave])
        timer = CAVE_SPAWN_INTERVALS[currentWave] - time_since_previous_wave;

    snprintf(extra_buffer, 32, "Wave %d: %d", currentWave + 1, timer);
    extra_color = ImColor(255, 255, 255);

    return true;
}

void TimerWidget::DisplayDialogue(GW::Packet::StoC::DisplayDialogue* packet) {
    if (GW::Map::GetMapID() != GW::Constants::MapID::Domain_of_Anguish) return;
    if (packet->message[1] != 0x5765) return;
    cave_start = GW::Map::GetInstanceTime();
}
