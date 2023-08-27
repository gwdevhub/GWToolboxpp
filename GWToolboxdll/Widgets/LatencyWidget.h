#pragma once

#include <GWCA/Utilities/Hook.h>

#include <ToolboxWidget.h>

class LatencyWidget : public ToolboxWidget {
    LatencyWidget() = default;
    ~LatencyWidget() override = default;

    GW::HookEntry Ping_Entry;
    int red_threshold = 250;
    bool show_avg_ping = false;
    int font_size = 0;

public:
    static LatencyWidget& Instance()
    {
        static LatencyWidget instance;
        return instance;
    }


    [[nodiscard]] const char* Name() const override { return "Latency"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_STOPWATCH; }

    void Initialize() override;
    void Update(float delta) override;

    static void OnServerPing(GW::HookStatus*, void* packet);

    static uint32_t GetPing();
    static uint32_t GetAveragePing();

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void LoadSettings(ToolboxIni* ini) override;

    void SaveSettings(ToolboxIni* ini) override;

    void DrawSettingsInternal() override;

    static ImColor GetColorForPing(uint32_t ping);
    static void SendPing(const wchar_t* = nullptr, int argc = 0, LPWSTR* argv = nullptr);
};
