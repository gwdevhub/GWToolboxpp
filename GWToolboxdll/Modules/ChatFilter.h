#pragma once

#include <ToolboxModule.h>

typedef long clock_t;

class ChatFilter : public ToolboxModule {
    ChatFilter() = default;

    ~ChatFilter() override = default;

public:
    static ChatFilter& Instance()
    {
        static ChatFilter instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Chat Filter"; }
    [[nodiscard]] const char* SettingsName() const override { return "Chat Settings"; }

    // MSVC can't reflect member names of internal-linkage types, so settings structs are nested in the class
    struct Settings {
        bool guild_announcement = false;
        bool self_drop_rare = false;
        bool self_drop_common = false;
        bool ally_drop_rare = false;
        bool ally_drop_common = false;
        bool ally_pickup_rare = false;
        bool ally_pickup_common = false;
        bool player_pickup_rare = false;
        bool player_pickup_common = false;
        bool salvage_messages = false;
        bool skill_points = false;
        bool pvp_messages = true;
        bool hoh_messages = false;
        bool favor = false;
        bool ninerings = true;
        bool noonehearsyou = true;
        bool lunars = true;
        bool away = false;
        bool you_have_been_playing_for = false;
        bool player_has_achieved_title = false;
        bool faction_gain = false;
        bool challenge_mission_messages = false;
        bool block_messages_from_inactive_channels = false;
        bool ashes_dropped = false;

        // Error messages on-screen
        bool invalid_target = false; // Includes other error messages, see ChatFilter.cpp.
        bool opening_chest_messages = false;
        bool inventory_is_full = false;
        bool item_cannot_be_used = false; // Includes other error messages, see ChatFilter.cpp.
        bool not_enough_energy = false;   // Includes other error messages, see ChatFilter.cpp.
        bool targetting_messages_from_me = false;   // Includes other error messages, see ChatFilter.cpp.
        bool targetting_messages_from_others = false;   // Includes other error messages, see ChatFilter.cpp.
        bool item_already_identified = false;

        bool messagebycontent = false;
        // Which channels to filter.
        bool filter_channel_local = true;
        bool filter_channel_guild = false;
        bool filter_channel_team = false;
        bool filter_channel_trade = true;
        bool filter_channel_alliance = false;
        bool filter_channel_emotes = false;
    };

    void Initialize() override;
    void Terminate() override;
    static void BlockMessageForMs(const wchar_t* message_contains, clock_t ms);
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;

    void Update(float delta) override;
};
