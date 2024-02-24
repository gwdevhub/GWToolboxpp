#pragma once

#include <GWCA/Constants/Skills.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Packets/StoC.h>
#include <chrono>

#include <ToolboxWidget.h>

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

    void Initialize() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
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
