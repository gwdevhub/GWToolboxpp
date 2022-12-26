#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Packets/StoC.h>

#include <ToolboxWidget.h>

class AlcoholWidget : public ToolboxWidget {
    AlcoholWidget() = default;
    ~AlcoholWidget() = default;
private:
    DWORD alcohol_level = 0;
    time_t last_alcohol = 0;
    long alcohol_time = 0;
    bool only_show_when_drunk = false;
    uint32_t GetAlcoholTitlePointsGained(); // Returns amount of alcohol points gained since last check (or map load)
    uint32_t prev_alcohol_title_points = 0; // Used in GetAlcoholTitlePointsGained
    GW::Constants::MapID map_id = GW::Constants::MapID::None;
    DWORD prev_packet_tint_6_level=0; // Record what last post processing packet was - for lunars check
    GW::HookEntry PostProcess_Entry;
public:
    static AlcoholWidget& Instance() {
        static AlcoholWidget instance;
        return instance;
    }

    const char* Name() const override { return "Alcohol"; }
    const char* Icon() const override { return ICON_FA_BEER; }

    void Initialize() override;
    void Update(float delta) override;
    uint32_t GetAlcoholLevel();
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    uint32_t GetAlcoholTitlePoints(); // Gets current alcohol title points.
    static void AlcUpdate(GW::HookStatus *, GW::Packet::StoC::PostProcess *packet);

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void DrawSettingInternal() override;
};
