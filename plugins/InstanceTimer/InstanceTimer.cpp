#include "InstanceTimer.h"

#include <filesystem>

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Defines.h>
#include <Timer.h>
#include <Utils/GuiUtils.h>
#include <GWCA/Utilities/Scanner.h>
#include <GWCA/GWCA.h>
#include <GWCA/Utilities/Hooker.h>

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static InstanceTimer instance;
    return &instance;
}

void InstanceTimer::LoadSettings(const wchar_t* folder)
{
    const auto inifile = std::filesystem::path(folder) / L"instancetimer.ini";
    ini.LoadFile(inifile.c_str());
    click_to_print_time = ini.GetBoolValue(Name(), VAR_NAME(click_to_print_time), click_to_print_time);
    show_extra_timers = ini.GetBoolValue(Name(), VAR_NAME(show_extra_timers), show_extra_timers);
}

void InstanceTimer::SaveSettings(const wchar_t* folder)
{
    const auto inifile = std::filesystem::path(folder) / L"instancetimer.ini";
    ini.SetBoolValue(Name(), VAR_NAME(click_to_print_time), click_to_print_time);
    ini.SetBoolValue(Name(), VAR_NAME(show_extra_timers), show_extra_timers);
    ini.SaveFile(inifile.c_str());
}

void InstanceTimer::DrawSettings()
{
    if (!toolbox_handle) return;
    ImGui::Checkbox("Ctrl + Click to print time", &click_to_print_time);
    ImGui::Checkbox("Show extra timers", &show_extra_timers);

    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Such as Deep Aspects");
    }
}

void InstanceTimer::Initialize(ImGuiContext* ctx, ImGuiAllocFns fns, HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, fns, toolbox_dll);
    GW::Scanner::Initialize();
    GW::Initialize();
}
void InstanceTimer::SignalTerminate() {
    GW::DisableHooks();
}
bool InstanceTimer::CanTerminate() {
    return GW::HookBase::GetInHookCount() == 0;
}
void InstanceTimer::Terminate()
{
    ToolboxPlugin::Terminate();
    GW::Terminate();
}

void InstanceTimer::Draw(IDirect3DDevice9*)
{
    if (!toolbox_handle) return;
    if (!visible) return;
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;

    const auto time = GW::Map::GetInstanceTime() / 1000;

    const bool ctrl_pressed = ImGui::IsKeyDown(ImGuiKey_ModCtrl);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(250.0f, 90.0f), ImGuiCond_FirstUseEver);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;
    if (ImGui::Begin(Name(), nullptr, flags)) {
        snprintf(timer_buffer, 32, "%d:%02d:%02d", time / (60 * 60), time / 60 % 60, time % 60);
        ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::widget_large));
        const auto cursor_pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cursor_pos.x + 2, cursor_pos.y + 2));
        ImGui::TextColored(ImColor(0, 0, 0), "%s", timer_buffer);
        ImGui::SetCursorPos(cursor_pos);
        ImGui::Text("%s", timer_buffer);
        ImGui::PopFont();

        if (show_extra_timers && (GetUrgozTimer() || (GetDeepTimer() || GetDhuumTimer() || GetTrapTimer()))) {
            ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::widget_small));
            const ImVec2 cur2 = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(cur2.x + 2, cur2.y + 2));
            ImGui::TextColored(ImColor(0, 0, 0), "%s", extra_buffer);
            ImGui::SetCursorPos(cur2);
            ImGui::TextColored(extra_color, "%s", extra_buffer);
            ImGui::PopFont();
        }

        if (click_to_print_time) {
            const ImVec2 size = ImGui::GetWindowSize();
            const ImVec2 min = ImGui::GetWindowPos();
            const ImVec2 max(min.x + size.x, min.y + size.y);
            if (ctrl_pressed && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(min, max)) {
                GW::Chat::SendChat('/', "age");
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

bool InstanceTimer::GetUrgozTimer()
{
    if (GW::Map::GetMapID() != GW::Constants::MapID::Urgozs_Warren) return false;
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) return false;
    const auto time = GW::Map::GetInstanceTime() / 1000;
    const int temp = (time - 1) % 25;
    if (temp < 15) {
        snprintf(extra_buffer, 32, "Open - %d", 15 - temp);
        extra_color = ImColor(0, 255, 0);
    }
    else {
        snprintf(extra_buffer, 32, "Closed - %d", 25 - temp);
        extra_color = ImColor(255, 0, 0);
    }
    return true;
}

bool InstanceTimer::GetDeepTimer()
{
    using namespace GW::Constants;

    if (GW::Map::GetMapID() != MapID::The_Deep) return false;
    if (GW::Map::GetInstanceType() != InstanceType::Explorable) return false;

    const auto agent_effects = GW::Effects::GetPlayerEffectsArray();
    if (!agent_effects) return false;
    GW::EffectArray& effects = agent_effects->effects;
    if (!effects.valid()) return false;

    static clock_t start = -1;
    SkillID skill = SkillID::No_Skill;
    for (const auto& effect : effects) {
        const SkillID effect_id = effect.skill_id;
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

    const clock_t diff = TIMER_DIFF(start) / 1000;

    // a 30s timer starts when you enter the aspect
    // a 30s timer starts 100s after you enter the aspect
    // a 30s timer starts 200s after you enter the aspect
    long timer = 30 - (diff % 30);
    if (diff > 100) timer = std::min(timer, 30 - ((diff - 100) % 30));
    if (diff > 200) timer = std::min(timer, 30 - ((diff - 200) % 30));
    switch (skill) {
        case SkillID::Aspect_of_Exhaustion: snprintf(extra_buffer, 32, "Exhaustion: %d", timer); break;
        case SkillID::Aspect_of_Depletion_energy_loss: snprintf(extra_buffer, 32, "Depletion: %d", timer); break;
        case SkillID::Scorpion_Aspect: snprintf(extra_buffer, 32, "Scorpion: %d", timer); break;
        default: break;
    }
    extra_color = ImColor(255, 255, 255);
    return true;
}

bool InstanceTimer::GetDhuumTimer()
{
    // todo: implement
    return false;
}

bool InstanceTimer::GetTrapTimer()
{
    using namespace GW::Constants;
    if (GW::Map::GetInstanceType() != InstanceType::Explorable) return false;
    const auto time = GW::Map::GetInstanceTime() / 1000;
    const auto temp = time % 20;
    int timer;
    if (temp < 10) {
        timer = 10 - temp;
        extra_color = ImColor(0, 255, 0);
    }
    else {
        timer = 20 - temp;
        extra_color = ImColor(255, 0, 0);
    }

    switch (GW::Map::GetMapID()) {
        case MapID::Catacombs_of_Kathandrax_Level_1:
        case MapID::Catacombs_of_Kathandrax_Level_2:
        case MapID::Catacombs_of_Kathandrax_Level_3:
        case MapID::Bloodstone_Caves_Level_1:
        case MapID::Arachnis_Haunt_Level_2:
        case MapID::Oolas_Lab_Level_2: snprintf(extra_buffer, 32, "Fire Jet: %d", timer); return true;
        case MapID::Heart_of_the_Shiverpeaks_Level_3: snprintf(extra_buffer, 32, "Fire Spout: %d", timer); return true;
        case MapID::Shards_of_Orr_Level_3:
        case MapID::Cathedral_of_Flames_Level_3: snprintf(extra_buffer, 32, "Fire Trap: %d", timer); return true;
        case MapID::Sepulchre_of_Dragrimmar_Level_1:
        case MapID::Ravens_Point_Level_1:
        case MapID::Ravens_Point_Level_2:
        case MapID::Heart_of_the_Shiverpeaks_Level_1:
        case MapID::Darkrime_Delves_Level_2: snprintf(extra_buffer, 32, "Ice Jet: %d", timer); return true;
        case MapID::Darkrime_Delves_Level_1:
        case MapID::Secret_Lair_of_the_Snowmen: snprintf(extra_buffer, 32, "Ice Spout: %d", timer); return true;
        case MapID::Bogroot_Growths_Level_1:
        case MapID::Arachnis_Haunt_Level_1:
        case MapID::Shards_of_Orr_Level_1:
        case MapID::Shards_of_Orr_Level_2: snprintf(extra_buffer, 32, "Poison Jet: %d", timer); return true;
        case MapID::Bloodstone_Caves_Level_2: snprintf(extra_buffer, 32, "Poison Spout: %d", timer); return true;
        case MapID::Cathedral_of_Flames_Level_2:
        case MapID::Bloodstone_Caves_Level_3: snprintf(extra_buffer, 32, "Poison Trap: %d", timer); return true;
        default: return false;
    }
}
