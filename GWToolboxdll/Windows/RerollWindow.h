#pragma once

#include "ToolboxWindow.h"
#include <GWCA/GameEntities/Friendslist.h>

namespace GW {
    namespace Constants {
        enum class MapID;
    }
}

class RerollWindow : public ToolboxWindow {
    RerollWindow() = default;
    ~RerollWindow() {
        for (const auto& it : account_characters) {
            delete it.second;
        }
        account_characters.clear();
        if (guild_hall_uuid) {
            delete guild_hall_uuid;
            guild_hall_uuid = nullptr;
        }
    }

public:
    static RerollWindow& Instance() {
        static RerollWindow instance;
        return instance;
    }

    const char* Name() const override { return "Reroll"; }
    const char* Icon() const override { return ICON_FA_USERS; }

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void Initialize() override;
    void Update(float) override;
    std::vector<std::wstring>* GetAvailableChars();

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;


    // Hook to override status on login - allows us to keep FL status across rerolls without messing with UI
    static void OnSetStatus(GW::FriendStatus status);

    static void CmdReroll(const wchar_t* message, int argc, LPWSTR* argv);

    bool Reroll(wchar_t* character_name, bool same_map = true, bool same_party = true);
    bool Reroll(wchar_t* character_name, GW::Constants::MapID _map_id);
private:

    bool travel_to_same_location_after_rerolling = true;
    bool rejoin_party_after_rerolling = true;

    bool check_available_chars = true;

    // Can find out campaign etc from props array
    struct AvailableCharacterInfo {
        /* + h0000 */ uint32_t h0000[2];
        /* + h0008 */ uint32_t uuid[4];
        /* + h0018 */ wchar_t player_name[20];
        /* + h0040 */ uint32_t props[17];
        uint32_t map_id() {
            return (props[0] & 0xffff0000);
        }
        uint32_t primary() {
            return (props[2] & 0x00f00000) >> 20;
        }
        uint32_t campaign() {
            return (props[7] & 0x000f0000) >> 16;
        }
        uint32_t level() {
            return ((props[7] & 0x0ff00000) >> 20) - 64;
        }
    };
    static_assert(sizeof(AvailableCharacterInfo) == 0x84);
    GW::Array<AvailableCharacterInfo>* available_chars_ptr = 0;

    clock_t reroll_timeout = 0;
    clock_t reroll_stage_set = 0;
    uint32_t reroll_index_needed = 0;
    uint32_t reroll_index_current = 0xffffffdd;
    GW::FriendStatus online_status = GW::FriendStatus::Online;
    GW::Constants::MapID map_id = (GW::Constants::MapID)0;
    int district_id = 0;
    int region_id = 0;
    int language_id = 0;
    uint32_t* guild_hall_uuid = 0;
    wchar_t initial_player_name[20] = { 0 };
    wchar_t reroll_to_player_name[20] = { 0 };
    wchar_t party_leader[20] = { 0 };
    bool same_map = false;
    bool same_party = false;
    const wchar_t* failed_message = 0;
    bool return_on_fail = false;
    bool reverting_reroll = false;
    enum RerollStage {
        None,
        PendingLogout,
        PromptPendingLogout,
        WaitingForCharSelect,
        CheckForCharname,
        NavigateToCharname,
        WaitForCharacterLoad,
        WaitForScrollableOutpost,
        WaitForActiveDistrict,
        WaitForMapLoad,
        WaitForEmptyParty,
        Done
    } reroll_stage = None;
    uint32_t reroll_scroll_from_map_id = 0;

    void RerollFailed(const wchar_t* reason);

    void RerollSuccess();

    std::map<std::wstring, std::vector<std::wstring>*> account_characters{};

    bool IsInMap(bool include_district = true);
    bool IsCharSelectReady();

    void AddAvailableCharacter(const wchar_t* email, const wchar_t* charname);

    GW::HookEntry OnGoToCharSelect_Entry;
    static void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage msg_id, void*, void*);
};
