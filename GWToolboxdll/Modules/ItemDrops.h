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
        uint32_t model_id = 0;
        uint32_t model_file_id = 0;
        uint32_t interaction = 0;
        uint16_t quantity = 1;
        uint16_t value = 0;
        uint16_t player_count = 1;
        uint8_t hero_count = 0;
        uint8_t henchman_count = 0;
        IDirect3DTexture9** icon = 0;
        GW::Constants::ItemType type;
        GW::Constants::Rarity rarity = GW::Constants::Rarity::Unknown;
        bool hard_mode = false;
        GuiUtils::EncString item_name;
        GW::Constants::MapID map_id = GW::Constants::MapID::None;
        GW::Constants::DamageType damage_type = GW::Constants::DamageType::None;
        uint8_t min_damage = 0;
        uint8_t max_damage = 0;
        GW::Constants::AttributeByte requirement_attribute = GW::Constants::AttributeByte::None;
        uint8_t requirement_value = 0;

        uint32_t instance_time = 0;
        time_t system_time = 0;

        PendingDrop(GW::Item*);
        const std::wstring toCSV();
        static const wchar_t* GetCSVHeader();
    };

    std::vector<PendingDrop*>& GetDropHistory();
    void ClearDropHistory();
    bool IsTrackingEnabled() const;
};
