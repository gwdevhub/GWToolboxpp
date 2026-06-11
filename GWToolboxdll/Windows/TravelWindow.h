#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <ToolboxWindow.h>

namespace GuiUtils {
    class EncString;
}
namespace GW {
    enum class Continent : uint32_t;
    struct AreaInfo;
    namespace Constants {
        enum class Language;
        enum class ServerRegion;
    }
}

class TravelWindow : public ToolboxWindow {
    TravelWindow() : ToolboxWindow() { show_menubutton = can_show_in_main_window; }
    ~TravelWindow() override = default;

public:
    static TravelWindow& Instance()
    {
        static TravelWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Travel"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_GLOBE_EUROPE; }

    struct Settings {
        bool close_on_travel = false;
        bool collapse_on_travel = false;
        bool retry_map_travel = false;
        bool search_in_english = true;
        bool show_zaishen_buttons = true;
    };

    // User-defined travel destination (shown as 2-column buttons in main window)
    struct UserDestEntry {
        GW::Constants::MapID map_id = GW::Constants::MapID::None;
        GW::Constants::District district = GW::Constants::District::Current;
        uint8_t district_number = 0;
    };

    // Custom shorthand alias for the /tp command
    struct AliasEntry {
        std::string alias;
        GW::Constants::MapID map_id = GW::Constants::MapID::None;
        GW::Constants::District district = GW::Constants::District::Current;
        uint8_t district_number = 0;
    };

    void Initialize() override;

    void Terminate() override;

    bool TravelFavorite(unsigned int idx);

    // Travel with checks, returns false if already in outpost or outpost not unlocked
    bool Travel(GW::Constants::MapID map_id, GW::Constants::District district = GW::Constants::District::Current, uint32_t district_number = 0);

    bool TravelNearest(const GW::Constants::MapID map_id);

    // Travel to relevent outpost, then use scroll to access Urgoz/Deep
    void ScrollToOutpost(
        GW::Constants::MapID outpost_id,
        GW::Constants::District district = GW::Constants::District::Current,
        uint32_t district_number = 0);

    static bool IsWaitingForMapTravel();

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void Update(float delta) override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;
    static GW::Constants::MapID GetNearestOutpostToLocation(const GW::AreaInfo* origin, const GW::Vec2f& world_map_pos);
    static GW::Constants::MapID GetNearestOutpostToPlayer();
    static GW::Constants::MapID GetNearestOutpost(GW::Constants::MapID map_to);

private:
    void TravelButton(GW::Constants::MapID mapid, int x_idx, GW::Constants::District dest_district = GW::Constants::District::Current, uint32_t dest_district_number = 0) const;
    // ==== Travel variables ====
    GW::Constants::District district = GW::Constants::District::Current;
    uint32_t district_number = 0;
};
