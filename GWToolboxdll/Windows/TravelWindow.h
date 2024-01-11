#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <ToolboxWindow.h>

namespace GuiUtils {
    class EncString;
}
namespace GW {
    namespace Constants {
        enum class Language;
        enum class ServerRegion;
    }
}

class TravelWindow : public ToolboxWindow {
    TravelWindow() = default;
    ~TravelWindow() override = default;

public:
    static TravelWindow& Instance()
    {
        static TravelWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Travel"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_GLOBE_EUROPE; }

    void Initialize() override;

    void Terminate() override;

    bool TravelFavorite(unsigned int idx);

    // Travel with checks, returns false if already in outpost or outpost not unlocked
    bool Travel(GW::Constants::MapID map_id, GW::Constants::District district, uint32_t district_number = 0);

    // Travel to relevent outpost, then use scroll to access Urgoz/Deep
    void ScrollToOutpost(
        GW::Constants::MapID outpost_id,
        GW::Constants::District district = GW::Constants::District::Current,
        uint32_t district_number = 0);

    static bool IsWaitingForMapTravel();

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void Update(float delta) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
    static GW::Constants::MapID GetNearestOutpost(GW::Constants::MapID map_to);

private:
    void TravelButton(const char* text, const int x_idx, const GW::Constants::MapID mapid);
    // ==== Travel variables ====
    GW::Constants::District district = GW::Constants::District::Current;
    uint32_t district_number = 0;
};
