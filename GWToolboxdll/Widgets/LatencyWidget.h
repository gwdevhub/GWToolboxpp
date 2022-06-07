#pragma once

#include <GWCA/Constants/Maps.h>
#include <GWCA/Packets/StoC.h>
#include <GWCA/Utilities/Hook.h>

#include <ToolboxWidget.h>

class LatencyWidget : public ToolboxWidget {
    LatencyWidget(){};
    ~LatencyWidget(){};

private:
    GW::HookEntry Ping_Entry;
    int red_threshold = 250;

public:
    static LatencyWidget& Instance() {
        static LatencyWidget instance;
        return instance;
    }
    uint32_t ping_ms = 0;

    const char* Name() const override { return "Latency"; }
    const char* Icon() const override { return ICON_FA_STOPWATCH; }

    void Initialize() override;
    void Update(float delta) override;

    static void PingUpdate(GW::HookStatus*, void* packet);

    uint32_t GetPing();

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void LoadSettings(CSimpleIni* ini);

    void SaveSettings(CSimpleIni* ini);

    void DrawSettingInternal();

    static ImColor GetColorForPing(uint32_t ping);
    static void pingping(const wchar_t*, int argc, LPWSTR* argv);
};