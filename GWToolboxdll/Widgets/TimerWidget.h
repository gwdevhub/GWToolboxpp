#pragma once

#include <GWCA/Packets/StoC.h>
#include <chrono>

#include <ToolboxWidget.h>
#include <Utils/FontLoader.h>

class TimerWidget : public ToolboxWidget {
    ~TimerWidget() override = default;

public:
    static TimerWidget& Instance()
    {
        static TimerWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Timer"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_STOPWATCH; }

    struct Settings {
        bool hide_in_outpost = false;
        bool show_deep_timer = true;
        bool show_urgoz_timer = true;
        bool show_doa_timer = true;
        bool show_dhuum_timer = true;
        bool show_dungeon_traps_timer = true;
        bool show_spirit_timers = true;
        float font_size = static_cast<float>(FontLoader::FontSize::widget_large);
        float font_size_extra_timers = static_cast<float>(FontLoader::FontSize::widget_label);
        bool use_instance_timer = false;
        bool never_reset = false;
        bool stop_at_objective_completion = true;
        bool also_show_instance_timer = false;
        int show_decimals = 1;
        bool click_to_print_time = false;
        bool print_time_zoning = false;
        bool print_time_objective = true;
    };

    void Initialize() override;
    void Terminate() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;
    [[nodiscard]] ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags = 0, bool noinput_if_frozen = true) const override;

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
    [[nodiscard]] unsigned long GetStartPoint() const;
    void PrintTimer(); // prints current timer to chat

    void OnPreGameSrvTransfer(GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer* pak);
    void OnPostGameSrvTransfer(GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer* pak);

private:

};
