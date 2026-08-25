#pragma once

#include <ToolboxWindow.h>

class PlaystyleRestrictionsWindow : public ToolboxWindow {
    PlaystyleRestrictionsWindow() = default;
    ~PlaystyleRestrictionsWindow() override = default;

public:
    static PlaystyleRestrictionsWindow& Instance()
    {
        static PlaystyleRestrictionsWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Playstyle Restrictions"; }
    [[nodiscard]] const char* Description() const override { return "Enforce a shareable profile of self-imposed challenge-run restrictions"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_LOCK; }

    struct RequiredTitle {
        uint32_t title_id = 0; // GW::Constants::TitleID
        uint32_t min_tier = 1;
    };

    // The shareable ruleset. Lives in its own json file, not in the toolbox settings doc,
    // so it can be handed to another player verbatim.
    struct Profile {
        std::string profile_name = "Unnamed profile";
        std::string author;
        std::string notes;

        std::vector<uint32_t> campaign_order = {1, 2, 3, 4}; // GW::Constants::Campaign
        bool enforce_campaign_order = false;
        bool block_mission_skipping = false;
        bool require_campaign_titles_maxed = false;
        bool require_hard_mode = false;
        bool require_mission_bonuses = false;
        bool gate_presearing_exit = false;
        std::vector<RequiredTitle> required_presearing_titles = {{22, 1}, {9, 1}}; // LDoA, Survivor
        uint32_t min_level_for_gated_areas = 0;

        bool restrict_heroes_to_unlocked_campaigns = false;
        uint32_t max_party_heroes = 0; // 0 = uncapped
        bool lock_hero_skillbars = false;
        std::vector<uint32_t> hero_allow_list; // GW::Constants::HeroID, always permitted
        std::vector<uint32_t> hero_block_list;

        bool restrict_skills_to_unlocked_campaigns = false;
        bool single_attribute_line = false;
        bool lock_skillbar = false;
        bool block_elite_tomes = false;

        bool block_consumables = false;
        bool block_scrolls = false;
        bool lockpicks_hard_mode_only = false;
        bool block_purchased_runes = false; // major/superior only; loot-dropped ones stay legal
        bool block_zaishen_coin_purchase = false;
        std::vector<uint32_t> blocked_item_model_ids; // bonus/holiday weapons etc

        bool block_xunlai_chest = false;
    };

    struct Settings {
        bool enforce = true;
    };

    void Initialize() override;
    void Terminate() override;
    void Update(float delta) override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingsInternal() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
};
