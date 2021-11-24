#pragma once

#include "ToolboxWindow.h"

class RerollWindow : public ToolboxWindow {
    RerollWindow() {};
    ~RerollWindow();
public:
    static RerollWindow& Instance() {
        static RerollWindow instance;
        return instance;
    }

    const char* Name() const override { return "Reroll"; }
    const char* Icon() const override { return ICON_FA_CLIPBOARD; }

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void Update(float) override;
    std::vector<std::wstring>* GetAvailableChars();

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;

    void Reroll(wchar_t* character_name, bool same_map = true, bool same_party = true);

private:

    bool travel_to_same_location_after_rerolling = true;
    bool rejoin_party_after_rerolling = true;

    bool check_available_chars = true;
    wchar_t current_account_email[128] = { 0 };

    clock_t reroll_timeout = 0;
    uint32_t reroll_index_needed = 0;
    uint32_t reroll_index_current = 0xffffffdd;
    GW::Constants::MapID map_id = GW::Constants::MapID::None;
    int district_id = 0;
    int region_id = 0;
    int language_id = 0;
    uint32_t* guild_hall_uuid = 0;
    wchar_t initial_player_name[20] = { 0 };
    wchar_t reroll_to_player_name[20] = { 0 };
    wchar_t party_leader[20] = { 0 };
    bool same_map = false;
    bool same_party = false;
    wchar_t* failed_message = 0;
    bool reverting_reroll = false;
    enum RerollStage {
        None,
        PendingLogout,
        WaitingForCharSelect,
        CheckForCharname,
        NavigateToCharname,
        WaitForCharacterLoad,
        WaitForActiveDistrict,
        WaitForMapLoad,
        Done
    } reroll_stage = None;

    void RerollFailed(wchar_t* reason);

    void RerollSuccess();

    std::map<std::wstring, std::vector<std::wstring>*> account_characters;

    bool IsInMap(bool include_district = true);
    bool IsCharSelectReady();
    
};
