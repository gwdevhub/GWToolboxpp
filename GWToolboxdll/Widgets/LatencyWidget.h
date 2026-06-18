#pragma once

#include <ToolboxWidget.h>

class LatencyWidget : public ToolboxWidget {
    LatencyWidget() = default;
    ~LatencyWidget() override = default;


public:
    static LatencyWidget& Instance()
    {
        static LatencyWidget instance;
        return instance;
    }


    [[nodiscard]] const char* Name() const override { return "Latency"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_STOPWATCH; }

    struct Settings {
        int red_threshold = 250;
        bool show_avg_ping = false;
        float text_size = 40.f;
    };

    static void SendPing();
    static uint32_t GetPing();
    static uint32_t GetAveragePing();

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;

    void DrawSettingsInternal() override;

    static ImColor GetColorForPing(uint32_t ping);
};
