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

    void Initialize() override;
    void Update(float) override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    struct PendingDrop {
        uint32_t instance_time = 0;
        time_t system_time = 0;
        IDirect3DTexture9** icon = 0;
        wchar_t* item_name_enc = 0;
        GW::Constants::MapID map_id = GW::Constants::MapID::None;

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
    static_assert(sizeof(PendingDrop) == 40);

    std::vector<PendingDrop*>& GetDropHistory();
    int GetTotalGoldValue();
    void ClearDropHistory();
    bool IsTrackingEnabled() const;
};
