#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <ToolboxWindow.h>

namespace GuiUtils {
    class EncString;
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

    // Travel via UI interface, allowing "Are you sure" dialogs
    static void UITravel(GW::Constants::MapID MapID, GW::Constants::District district,
        uint32_t district_number = 0);

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
    static int RegionFromDistrict(GW::Constants::District district);
    static int LanguageFromDistrict(GW::Constants::District district);
    static GW::Constants::MapID GetNearestOutpost(GW::Constants::MapID map_to);

    static void CmdTP(const wchar_t* message, int argc, const LPWSTR* argv);

private:
    // ==== Helpers ====
    void TravelButton(const char* text, int x_idx, GW::Constants::MapID mapid);
    static GW::Constants::MapID IndexToOutpostID(int index);
    static bool ParseDistrict(const std::wstring& s, GW::Constants::District& district, uint32_t& number);
    static bool ParseOutpost(const std::wstring& s, GW::Constants::MapID& outpost, GW::Constants::District& district, const uint32_t& number);
    bool PlayerHasAnyMissingOutposts(const bool presearing) const;
    void DrawMissingOutpostsList(const bool presearing) const;

    // ==== Travel variables ====
    GW::Constants::District district = GW::Constants::District::Current;
    uint32_t district_number = 0;

    // ==== Favorites ====
    int fav_count = 0;
    std::vector<int> fav_index{};

    // ==== options ====
    bool close_on_travel = false;

    // ==== scroll to outpost ====
    GW::Constants::MapID scroll_to_outpost_id = GW::Constants::MapID::None;   // Which outpost do we want to end up in?
    GW::Constants::MapID scroll_from_outpost_id = GW::Constants::MapID::None; // Which outpost did we start from?

    bool map_travel_countdown_started = false;
    bool pending_map_travel = false;

    IDirect3DTexture9** scroll_texture = nullptr;

    /* Not used, but good to keep for reference!
    enum error_message_ids {
        error_B29 = 52,
        error_B30,
        error_B31,
        error_B32,
        error_B33,
        error_B34,
        error_B35,
        error_B36,
        error_B37,
        error_B38
    };
    enum error_message_trans_codes {
        error_B29 = 0xB29, // The target party has members who do not meet this mission's level requirements.
        error_B30, // You may not enter that outpost
        error_B31, // Your party leader must be a member of this guild. An officer from this guild must also be in the party.
        error_B32, // You must be the leader of your party to do that.
        error_B33, // You must be a member of a party to do that.
        error_B34, // Your party is already waiting to go somewhere else.
        error_B35, // Your party is already in that guild hall.
        error_B36, // Your party is already in that district.
        error_B37, // Your party is already in the active district.
        error_B38, // The merged party would be too large.
    };  */
};
