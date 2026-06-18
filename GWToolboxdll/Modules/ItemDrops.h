#pragma once

#include <ToolboxModule.h>
#include "InventoryManager.h"
#include <Utils/ToolboxUtils.h>


class ItemDrops : public ToolboxModule {
public:
    static ItemDrops& Instance()
    {
        static ItemDrops instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Item Filter"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_COINS; }
    [[nodiscard]] const char* SettingsName() const override { return "Item Settings"; }

    struct Settings {
        bool hide_player_white = false;
        bool hide_player_blue = false;
        bool hide_player_purple = false;
        bool hide_player_gold = false;
        bool hide_player_green = false;
        bool hide_party_white = false;
        bool hide_party_blue = false;
        bool hide_party_purple = false;
        bool hide_party_gold = false;
        bool hide_party_green = false;
        bool track_drops = false;
    };

    void Initialize() override;
    void Update(float) override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;

    struct PendingDrop {
        uint32_t instance_time = 0;
        time_t system_time = 0;
        IDirect3DTexture9** icon = 0;
        wchar_t* item_name_enc = 0;
        GW::Constants::MapID map_id = GW::Constants::MapID::None;

        // ModelFileID is the unique skin identifier; useful when one display
        // name covers many skins (e.g. "Storm Artifact" has 42+ variants).
        uint32_t model_file_id = 0;

        uint16_t value = 0;

        uint8_t quantity = 1;
        uint8_t min_damage = 0;
        uint8_t max_damage = 0;
        uint8_t player_count : 4 = 0;
        uint8_t hero_count : 4 = 0;
        uint8_t henchman_count : 4 = 0;
        uint8_t requirement_value : 4 = 0;

        GW::Constants::ItemType type = GW::Constants::ItemType::Unknown;
        GW::Constants::Rarity rarity = GW::Constants::Rarity::Unknown;
        GW::Constants::DamageType damage_type = GW::Constants::DamageType::None;
        GW::Constants::AttributeByte requirement_attribute = GW::Constants::AttributeByte::None;
        bool hard_mode = false;

        PendingDrop(GW::Item*);
        ~PendingDrop();
        const std::wstring toCSV();
        static const wchar_t* GetCSVHeader();
        GuiUtils::EncString* GetItemName();
    };
    static_assert(sizeof(PendingDrop) == 48);

    std::vector<PendingDrop*>& GetDropHistory();
    int GetTotalGoldValue();
    void ClearDropHistory();
    bool IsTrackingEnabled() const;
    void AddPendingExport(std::string);
};
