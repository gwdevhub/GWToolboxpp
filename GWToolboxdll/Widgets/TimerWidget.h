#pragma once

#include <GWCA/Constants/Skills.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Packets/StoC.h>
#include <chrono>

#include <ToolboxWidget.h>

class TimerWidget : public ToolboxWidget {
    TimerWidget() {
        for (const auto& [skill_id, name] : spirit_effects) {
            if (!spirit_effects_enabled.contains(skill_id))
                spirit_effects_enabled[skill_id] = false;
        }
    };
    ~TimerWidget() = default;

public:
    static TimerWidget& Instance() {
        static TimerWidget instance;
        return instance;
    }
    const char* Name() const override { return "Timer"; }
    const char* Icon() const override { return ICON_FA_STOPWATCH; }

    void Initialize() override;
    void LoadSettings(ToolboxIni *ini) override;
    void SaveSettings(ToolboxIni *ini) override;
    void DrawSettingInternal() override;
    ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags = 0, bool noinput_if_frozen = true) const;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void SetRunCompleted(bool no_print = false); // call when the objectives are completed to stop the timer
    // Time in ms since the current instance was created, based on when the map loading screen was shown.
    std::chrono::milliseconds GetMapTimeElapsed();
    // Time in ms since the current run was started. May be different to when the current instance was created.
    std::chrono::milliseconds GetRunTimeElapsed();
    // See GetRunTimeElapsed
    std::chrono::milliseconds GetTimer();
    unsigned long GetTimerMs(); // time in milliseconds
    unsigned long GetMapTimeElapsedMs();
    unsigned long GetRunTimeElapsedMs();
    unsigned long GetStartPoint() const;
    void PrintTimer(); // prints current timer to chat

    void OnPreGameSrvTransfer(GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer* pak);
    void OnPostGameSrvTransfer(GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer* pak);

private:

    // those function write to extra_buffer and extra_color.
    // they return true if there is something to draw.
    bool GetUrgozTimer();
    bool GetDeepTimer();
    bool GetDhuumTimer();
    bool GetTrapTimer();
    bool GetDoATimer();
    bool GetSpiritTimer();

    std::map<GW::Constants::SkillID, const char*> spirit_effects{
        {GW::Constants::SkillID::Edge_of_Extinction,"EoE"},
        {GW::Constants::SkillID::Quickening_Zephyr,"QZ"},
        {GW::Constants::SkillID::Famine,"Famine"},
        {GW::Constants::SkillID::Symbiosis,"Symbiosis"},
        {GW::Constants::SkillID::Winnowing,"Winnowing"},
        {GW::Constants::SkillID::Frozen_Soil,"Frozen Soil"},
        {GW::Constants::SkillID::Union,"Union"},
        {GW::Constants::SkillID::Shelter,"Shelter"},
        {GW::Constants::SkillID::Displacement,"Displacement"},
        {GW::Constants::SkillID::Life,"Life"},
        {GW::Constants::SkillID::Recuperation,"Recuperation"},
        {GW::Constants::SkillID::Winds,"Winds"}
    };

    bool hide_in_outpost = false;
    bool show_deep_timer = true;
    bool show_urgoz_timer = true;
    bool show_doa_timer = true;
    bool show_dhuum_timer = true;
    bool show_dungeon_traps_timer = true;
    bool show_spirit_timers = true;
    std::map<GW::Constants::SkillID, bool> spirit_effects_enabled{
        {GW::Constants::SkillID::Edge_of_Extinction, true},
        {GW::Constants::SkillID::Quickening_Zephyr, true}
    };

    char timer_buffer[32] = "";
    char extra_buffer[32] = "";
    char spirits_buffer[128] = "";
    ImColor extra_color = 0;

    bool use_instance_timer = false;
    bool never_reset = false;
    bool stop_at_objective_completion = true;
    bool also_show_instance_timer = false;
    int show_decimals = 1;

    bool click_to_print_time = false;
    bool print_time_zoning = false;
    bool print_time_objective = true;

    bool reset_next_loading_screen = false;
    bool in_explorable = false;
    bool in_dungeon = false;

    std::chrono::steady_clock::time_point run_started, run_completed, instance_started;

    unsigned long cave_start = 0; // instance timer when cave started
    GW::HookEntry DisplayDialogue_Entry;
    GW::HookEntry PreGameSrvTransfer_Entry;
    GW::HookEntry InstanceTimer_Entry;
    GW::HookEntry PostGameSrvTransfer_Entry;
    const uint32_t CAVE_SPAWN_INTERVALS[12] = {12, 12, 12, 12, 12, 12, 10, 10, 10, 10, 10, 10};
};
