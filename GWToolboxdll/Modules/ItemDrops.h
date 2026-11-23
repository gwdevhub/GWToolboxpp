#pragma once

#include <filesystem>
#include <set>
#include <unordered_map>

#include <ToolboxModule.h>
#include "InventoryManager.h"
#include <Utils/ToolboxUtils.h>


class ItemDrops : public ToolboxModule {
public:
    // How DropTrackerWindow groups the drop list; shared with ItemDrops so the same key derivation
    // drives both the persistent running tallies here and the on-disk drill-down filter there.
    enum class GroupMode { None, ItemName, Map, Rarity, Type, Weapon };

    struct DropTally {
        uint64_t count = 0;
        uint64_t quantity = 0;
        int64_t gold_value = 0;
        uint16_t min_damage = UINT16_MAX;
        uint16_t max_damage = 0;
        std::set<std::string> requirements;
    };

    struct WeaponKey {
        GW::Constants::ItemType type = GW::Constants::ItemType::Unknown;
        GW::Constants::DamageType damage_type = GW::Constants::DamageType::None;
        bool operator==(const WeaponKey&) const = default;
    };
    struct WeaponKeyHash {
        size_t operator()(const WeaponKey& k) const noexcept
        {
            return (static_cast<size_t>(k.type) << 8) ^ static_cast<size_t>(k.damage_type);
        }
    };

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
        wchar_t* item_name_enc = 0;
        GW::Constants::MapID map_id = GW::Constants::MapID::None;

        // ModelFileID is the unique skin identifier; useful when one display
        // name covers many skins (e.g. "Storm Artifact" has 42+ variants).
        uint32_t model_file_id = 0;
        GW::DyeInfo dye{}; // kept (not the live GW::Item*) so GetIcon() can look the icon up lazily, on first draw

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
        bool is_composite_model = false; // (item->interaction & 4) at drop time; the only bit GetItemImage cares about
        bool is_female_char = false;     // controlled character's gender at drop time, for gendered armor models

        PendingDrop(GW::Item*);
        ~PendingDrop();
        const std::wstring toCSV();
        static const wchar_t* GetCSVHeader();
        GuiUtils::EncString* GetItemName();
        IDirect3DTexture9** GetIcon();
    };

    std::vector<PendingDrop*>& GetDropHistory();
    uint64_t GetSessionDropCount() const;
    int GetTotalGoldValue();
    // Keyed by each dimension's native identifier (not a formatted string) so tallying a drop and
    // filtering the on-disk drill-down scan are both plain enum/id comparisons, not string work.
    const std::unordered_map<std::string, DropTally>& GetTallyByItemName() const;
    const std::unordered_map<GW::Constants::MapID, DropTally>& GetTallyByMap() const;
    const std::unordered_map<GW::Constants::Rarity, DropTally>& GetTallyByRarity() const;
    const std::unordered_map<GW::Constants::ItemType, DropTally>& GetTallyByType() const;
    const std::unordered_map<WeaponKey, DropTally, WeaponKeyHash>& GetTallyByWeapon() const;
    static bool IsWeaponType(GW::Constants::ItemType type);
    static std::string WeaponCategoryName(GW::Constants::ItemType type, GW::Constants::DamageType damage_type);
    std::filesystem::path GetSessionCsvPath() const;
    void ClearDropHistory();
    bool IsTrackingEnabled() const;
    void AddPendingExport(std::string);
};
