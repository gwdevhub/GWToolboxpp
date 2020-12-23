#pragma once

#include <GWCA/Constants/Skills.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Packets/StoC.h>
#include <chrono>

#include <ToolboxWidget.h>

class TimerWidget : public ToolboxWidget {
    TimerWidget() {
        for (const auto& it : spirit_effects) {
            auto e = spirit_effects_enabled.find(it.first);
            if (e != spirit_effects_enabled.end())
                continue;
            spirit_effects_enabled[it.first] = false;
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
    void LoadSettings(CSimpleIni *ini) override;
    void SaveSettings(CSimpleIni *ini) override;
    void DrawSettingInternal() override;
    ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags = 0, bool noinput_if_frozen = true) const;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void SetRunCompleted(); // call when the objectives are completed to stop the timer
    std::chrono::milliseconds GetTimer(); 
    unsigned long GetTimerMs(); // time in milliseconds

private:
    // those function write to extra_buffer and extra_color.
    // they return true if there is something to draw.
    bool GetUrgozTimer();
    bool GetDeepTimer();
    bool GetDhuumTimer();
    bool GetTrapTimer();
    bool GetDoATimer();
    bool GetSpiritTimer();
    
    std::map<GW::Constants::SkillID, char*> spirit_effects{
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
    bool click_to_print_time = false;
    bool show_extra_timers = true;
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

    bool reset_next_loading_screen = false;
    bool in_explorable = false;
    bool in_dungeon = false;
    //clock_t run_started = 0; // system time of when run started
    //clock_t run_completed = 0; // set to non-zero when objectives are completed

    std::chrono::steady_clock::time_point run_started, run_completed;

    unsigned long cave_start = 0; // instance timer when cave started
    GW::HookEntry DisplayDialogue_Entry;
    GW::HookEntry GameSrvTransfer_Entry;
    const uint32_t CAVE_SPAWN_INTERVALS[12] = {12, 12, 12, 12, 12, 12, 10, 10, 10, 10, 10, 10};
};
