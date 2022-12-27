#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <GWCA/Packets/StoC.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Map.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <Utils/GuiUtils.h>
#include <Logger.h>
#include <Timer.h>

#include <Modules/GameSettings.h>
#include <Widgets/TimerWidget.h>

using namespace std::chrono;

namespace
{
    steady_clock::time_point now() { return steady_clock::now(); }

    bool instance_timer_valid = true;

    bool is_valid(const steady_clock::time_point& time) { return time.time_since_epoch().count(); }

    template <typename T>
    void print_time(T duration, int decimals, unsigned int bufsize, char* buf) {
        auto time = duration_cast<milliseconds>(duration);
        long long secs = duration_cast<seconds>(time).count() % 60;
        int mins = duration_cast<minutes>(time).count() % 60;
        int hrs = duration_cast<hours>(time).count();
        switch (decimals) {
            case 1: snprintf(buf, bufsize, "%d:%02d:%02lld.%01lld", hrs, mins, secs, time.count() / 100 % 10);
                break;
            case 2: snprintf(buf, bufsize, "%d:%02d:%02lld.%02lld", hrs, mins, secs, time.count() / 10 % 100);
                break;
            case 3: snprintf(buf, bufsize, "%d:%02d:%02lld.%03lld", hrs, mins, secs, time.count() % 1000);
                break;
            default: // and 0
                snprintf(buf, bufsize, "%d:%02d:%02lld", hrs, mins, secs);
                break;
        }
    }
}
// Called before map change
void TimerWidget::OnPreGameSrvTransfer(GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer*) {
    if (print_time_zoning && in_explorable && !is_valid(run_completed)) {
        // do this here, before we actually reset it
        PrintTimer();
    }
    run_completed = now();
}
// Called just after map change
void TimerWidget::OnPostGameSrvTransfer(GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer* pak) {
    cave_start = 0; // reset doa's cave timer
    instance_timer_valid = false;
    std::chrono::steady_clock::time_point now_tp = now();
    run_completed = std::chrono::steady_clock::time_point();

    instance_started = now_tp;

    // If reset_next_loading_screen, reset regardless of never_reset
    if (reset_next_loading_screen) {
        run_started = now_tp;
        reset_next_loading_screen = false;
    }

    if (!never_reset) {
        if (pak->is_explorable && !in_explorable) { // if zoning from outpost to explorable
            run_started = now_tp;
        }
        else if (!pak->is_explorable) { // zoning back to outpost
            run_started = now_tp;
        }

        GW::AreaInfo* info = GW::Map::GetMapInfo((GW::Constants::MapID)pak->map_id);
        if (info) {
            bool new_in_dungeon = (info->type == GW::RegionType::Dungeon);

            if (new_in_dungeon && !in_dungeon) { // zoning from explorable to dungeon
                run_started = now_tp;
            }

            in_dungeon = new_in_dungeon;
        }
    }
    if(!is_valid(run_started))
        run_started = now_tp;

    in_explorable = pak->is_explorable;
}
void TimerWidget::Initialize() {
    ToolboxWidget::Initialize();

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayDialogue>(&DisplayDialogue_Entry,
        [this](GW::HookStatus*, GW::Packet::StoC::DisplayDialogue* packet) -> void {
            if (GW::Map::GetMapID() != GW::Constants::MapID::Domain_of_Anguish) return;
            if (packet->message[1] != 0x5765) return;
            cave_start = GW::Map::GetInstanceTime();
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(&PreGameSrvTransfer_Entry,
        [](GW::HookStatus* status, GW::Packet::StoC::GameSrvTransfer* pak) -> void {
        Instance().OnPreGameSrvTransfer(status, pak);
        }, -0x10);

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(&PostGameSrvTransfer_Entry,
        [](GW::HookStatus* status, GW::Packet::StoC::GameSrvTransfer* pak) -> void {
            Instance().OnPostGameSrvTransfer(status, pak);
        },0x5);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceTimer>(&InstanceTimer_Entry,
        [this](GW::HookStatus*, GW::Packet::StoC::InstanceTimer*) -> void {
            instance_timer_valid = true;
        },5);
    in_explorable = GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
    GW::Chat::CreateCommand(L"resettimer", [this](const wchar_t*, int, LPWSTR*) {
        reset_next_loading_screen = true;
        Log::Info("Resetting timer at the next loading screen.");
    });
    GW::Chat::CreateCommand(L"timerreset", [this](const wchar_t*, int, LPWSTR*) {
        reset_next_loading_screen = true;
    });
    if (!is_valid(run_started))
        run_started = now() - milliseconds(GW::Map::GetInstanceTime());
}


void TimerWidget::LoadSettings(ToolboxIni *ini) {
    ToolboxWidget::LoadSettings(ini);
    hide_in_outpost = ini->GetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);
    use_instance_timer = ini->GetBoolValue(Name(), VAR_NAME(use_instance_timer), use_instance_timer);
    never_reset = ini->GetBoolValue(Name(), VAR_NAME(never_reset), never_reset);
    stop_at_objective_completion =
        ini->GetBoolValue(Name(), VAR_NAME(stop_at_objective_completion), stop_at_objective_completion);
    also_show_instance_timer = ini->GetBoolValue(Name(), VAR_NAME(also_show_instance_timer), also_show_instance_timer);
    show_decimals = ini->GetLongValue(Name(), VAR_NAME(show_decimals), show_decimals);
    show_decimals = std::clamp(show_decimals, 0, 3);
    click_to_print_time = ini->GetBoolValue(Name(), VAR_NAME(click_to_print_time), click_to_print_time);
    print_time_zoning = ini->GetBoolValue(Name(), VAR_NAME(print_time_zoning), print_time_zoning);
    print_time_objective = ini->GetBoolValue(Name(), VAR_NAME(print_time_objective), print_time_objective);
    bool show_extra_timers = ini->GetBoolValue(Name(), VAR_NAME(show_extra_timers), true);
    if (!show_extra_timers) {
        // Legacy
        show_deep_timer = false;
        show_urgoz_timer = false;
        show_doa_timer = false;
        show_dhuum_timer = false;
        show_dungeon_traps_timer = false;
    }
    else {
        show_deep_timer = ini->GetBoolValue(Name(), VAR_NAME(show_deep_timer), show_deep_timer);
        show_urgoz_timer = ini->GetBoolValue(Name(), VAR_NAME(show_urgoz_timer), show_urgoz_timer);
        show_doa_timer = ini->GetBoolValue(Name(), VAR_NAME(show_doa_timer), show_doa_timer);
        show_dhuum_timer = ini->GetBoolValue(Name(), VAR_NAME(show_dhuum_timer), show_dhuum_timer);
        show_dungeon_traps_timer = ini->GetBoolValue(Name(), VAR_NAME(show_dungeon_traps_timer), show_dungeon_traps_timer);
    }
    show_spirit_timers = ini->GetBoolValue(Name(), VAR_NAME(show_spirit_timers), show_spirit_timers);
    for (const auto& it : spirit_effects) {
        char ini_name[32];
        snprintf(ini_name, 32, "spirit_effect_%d", it.first);
        spirit_effects_enabled[it.first] = ini->GetBoolValue(Name(), ini_name, spirit_effects_enabled[it.first]);
    }
}

void TimerWidget::SaveSettings(ToolboxIni *ini) {
    ToolboxWidget::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(hide_in_outpost), hide_in_outpost);
    ini->SetBoolValue(Name(), VAR_NAME(use_instance_timer), use_instance_timer);
    ini->SetBoolValue(Name(), VAR_NAME(never_reset), never_reset);
    ini->SetBoolValue(Name(), VAR_NAME(stop_at_objective_completion), stop_at_objective_completion);
    ini->SetBoolValue(Name(), VAR_NAME(also_show_instance_timer), also_show_instance_timer);
    ini->SetLongValue(Name(), VAR_NAME(show_decimals), show_decimals);
    ini->SetBoolValue(Name(), VAR_NAME(click_to_print_time), click_to_print_time);
    ini->SetBoolValue(Name(), VAR_NAME(print_time_zoning), print_time_zoning);
    ini->SetBoolValue(Name(), VAR_NAME(print_time_objective), print_time_objective);
    ini->SetBoolValue(Name(), VAR_NAME(show_spirit_timers), show_spirit_timers);
    ini->SetBoolValue(Name(), VAR_NAME(show_deep_timer), show_deep_timer);
    ini->SetBoolValue(Name(), VAR_NAME(show_doa_timer), show_doa_timer);
    ini->SetBoolValue(Name(), VAR_NAME(show_urgoz_timer), show_urgoz_timer);
    ini->SetBoolValue(Name(), VAR_NAME(show_dhuum_timer), show_dhuum_timer);
    ini->SetBoolValue(Name(), VAR_NAME(show_dungeon_traps_timer), show_dungeon_traps_timer);
    for (const auto& it : spirit_effects) {
        char ini_name[32];
        snprintf(ini_name, 32, "spirit_effect_%d", it.first);
        ini->SetBoolValue(Name(), ini_name, spirit_effects_enabled[it.first]);
    }
}

void TimerWidget::DrawSettingInternal() {
    ToolboxWidget::DrawSettingInternal();

    ImGui::SameLine(); ImGui::Checkbox("Hide in outpost", &hide_in_outpost);
    if (ImGui::RadioButton("Instance timer", use_instance_timer)) {
        use_instance_timer = true;
    }
    if (ImGui::RadioButton("Real-time timer", !use_instance_timer)) {
        use_instance_timer = false;
    }
    ImGui::ShowHelp("Real-time timer does not reset when zoning between explorable areas.\n \
        You can use /resettimer to force a reset at the next loading screen.");
    ImGui::Indent();
    ImGui::Checkbox("Never reset", &never_reset);
    ImGui::ShowHelp(
        "Don't reset when entering outposts, explorables (from outposts), and dungeons. \n" \
        "Useful for timing longer runs.\n" \
        "Requires 'Use instance timer' above NOT ticked");
    ImGui::Checkbox("Stop at objective completion", &stop_at_objective_completion);
    ImGui::Checkbox("Also show instance timer", &also_show_instance_timer);
    ImGui::Unindent();
    if (ImGui::SliderInt("Show decimals", &show_decimals, 0, 3)) {
        show_decimals = std::clamp(show_decimals, 0, 3);
    }
    ImGui::Text("Print time:");
    ImGui::Indent();
    ImGui::StartSpacedElements(200.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("With Ctrl+Click on timer", &click_to_print_time);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("At objective completion", &print_time_objective);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("When leaving explorables", &print_time_zoning);
    ImGui::Unindent();

    ImGui::Text("Show extra timers:");
    ImGui::Indent();

    std::vector<std::pair<const char*, bool*>> timers = {
        { "Deep aspects",&show_deep_timer},
        { "DoA cave",&show_doa_timer},
        { "Dhuum",&show_dhuum_timer},
        { "Urgoz doors",&show_urgoz_timer},
        { "Dungeon traps",&show_dungeon_traps_timer}
    };
    ImGui::StartSpacedElements(140.f);
    for (size_t i = 0; i < timers.size();i++) {
        ImGui::NextSpacedElement();
        ImGui::Checkbox(timers[i].first, timers[i].second);
    }
    ImGui::Unindent();
    ImGui::Checkbox("Show spirit timers", &show_spirit_timers);
    ImGui::ShowHelp("Time until spirits die in seconds");
    if (show_spirit_timers) {
        ImGui::Indent();
        ImGui::StartSpacedElements(140.f);
        for (const auto& it : spirit_effects) {
            ImGui::NextSpacedElement();
            ImGui::Checkbox(it.second, &spirit_effects_enabled[it.first]);
        }
        ImGui::Unindent();
    }
}

std::chrono::milliseconds TimerWidget::GetMapTimeElapsed() {
    if (!is_valid(instance_started)) {
        // Can happen if toolbox was started after map load
        instance_started = now() - milliseconds(GW::Map::GetInstanceTime());
    }
    return duration_cast<milliseconds>(now() - instance_started);
}
std::chrono::milliseconds TimerWidget::GetTimer() {
    if (use_instance_timer) {
        return milliseconds(instance_timer_valid ? GW::Map::GetInstanceTime() : 0);
    }
    return GetRunTimeElapsed();
}
std::chrono::milliseconds TimerWidget::GetRunTimeElapsed()
{
    if (!is_valid(run_started)) {
        return milliseconds(0);
    } else if (is_valid(run_completed)) {
        return duration_cast<milliseconds>(run_completed - run_started);

    } else {
        return duration_cast<milliseconds>(now() - run_started);
    }
}
unsigned long TimerWidget::GetStartPoint() const
{
    const auto time_point = use_instance_timer ? instance_started : run_started;
    if (!is_valid(time_point)) {
        return static_cast<unsigned long>(-1);
    }
    return static_cast<unsigned long>(duration_cast<milliseconds>(time_point.time_since_epoch()).count());
}

unsigned long TimerWidget::GetTimerMs() { return static_cast<unsigned long>(GetTimer().count()); }
unsigned long TimerWidget::GetMapTimeElapsedMs() { return static_cast<unsigned long>(GetMapTimeElapsed().count()); }
unsigned long TimerWidget::GetRunTimeElapsedMs() { return static_cast<unsigned long>(GetRunTimeElapsed().count()); }


void TimerWidget::SetRunCompleted(const bool no_print)
{
    if (is_valid(run_started) && stop_at_objective_completion) {
        run_completed = now();
    }
    if (print_time_objective && is_valid(run_started) && is_valid(run_completed) && !no_print) {
        PrintTimer();
    }
}

void TimerWidget::PrintTimer()
{
    char buf[32];
    print_time(GetTimer(), 2, 32, buf);
    Log::Info("Time: %s", buf);
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

    const bool ctrl_pressed = ImGui::IsKeyDown(ImGuiKey_ModCtrl);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(250.0f, 90.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !(click_to_print_time && ctrl_pressed)))) {

        // Main timer:
        print_time(GetTimer(), show_decimals, 32, timer_buffer);
        ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::widget_large));
        ImGui::TextShadowed(timer_buffer, {2, 2});
        ImGui::PopFont();

        if (also_show_instance_timer) {
            print_time(milliseconds(GW::Map::GetInstanceTime()), show_decimals, 32, timer_buffer);
            ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::widget_large));
            ImGui::TextShadowed(timer_buffer, {2, 2});
            ImGui::PopFont();
        }

        auto drawTimer = [](char* buffer, ImColor* extra_color = 0) {
            ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::widget_label));
            ImVec2 cur2 = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(cur2.x + 1, cur2.y + 1));
            ImGui::TextColored(ImColor(0, 0, 0), buffer);
            ImGui::SetCursorPos(cur2);
            if (extra_color) {
                ImGui::TextColored(*extra_color, buffer);
            }
            else {
                ImGui::Text(buffer);
            }
            ImGui::PopFont();
        };
        if (show_deep_timer && GetDeepTimer())
            drawTimer(extra_buffer, &extra_color);
        if (show_urgoz_timer && GetUrgozTimer())
            drawTimer(extra_buffer, &extra_color);
        if (show_doa_timer && GetDoATimer())
            drawTimer(extra_buffer, &extra_color);
        if (show_dungeon_traps_timer && GetTrapTimer())
            drawTimer(extra_buffer, &extra_color);
        if (show_dhuum_timer && GetDhuumTimer())
            drawTimer(extra_buffer, &extra_color);
        if (show_spirit_timers && GetSpiritTimer())
            drawTimer(spirits_buffer);

        if (click_to_print_time) {
            ImVec2 size = ImGui::GetWindowSize();
            ImVec2 min = ImGui::GetWindowPos();
            ImVec2 max(min.x + size.x, min.y + size.y);
            if (ctrl_pressed && ImGui::IsMouseReleased(0) && ImGui::IsMouseHoveringRect(min, max)) {
                if (!GameSettings::GetSettingBool("auto_age2_on_age")) {
                    PrintTimer();
                }
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

    GW::EffectArray* effects = GW::Effects::GetPlayerEffects();
    if (!effects) return false;

    int offset = 0;
    for (auto& effect : *effects) {
        if (!effect.duration)
            continue;
        SkillID effect_id = effect.skill_id;
        auto spirit_effect_enabled = spirit_effects_enabled.find(effect_id);
        if (spirit_effect_enabled == spirit_effects_enabled.end() || !(spirit_effect_enabled->second))
            continue;
        offset += snprintf(&spirits_buffer[offset], sizeof(spirits_buffer) - offset, "%s%s: %d", offset ? "\n" : "", spirit_effects[effect_id], effect.GetTimeRemaining() / 1000);
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

    GW::EffectArray* effects = GW::Effects::GetPlayerEffects();
    if (!effects) return false;

    static clock_t start = -1;
    SkillID skill = SkillID::No_Skill;
    for (auto& effect : *effects) {
        SkillID effect_id = (SkillID)effect.skill_id;
        switch (effect_id) {
        case SkillID::Aspect_of_Exhaustion:
        case SkillID::Aspect_of_Depletion_energy_loss:
        case SkillID::Scorpion_Aspect:
            skill = effect_id;
            break;
        default:
            break;
        }
        if (skill != SkillID::No_Skill)
            break;
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
