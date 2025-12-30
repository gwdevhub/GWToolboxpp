#pragma once

#include <ToolboxModule.h>


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
    void SignalTerminate() override;
    bool CanTerminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    struct PendingDrop {
        uint32_t item_id;
        uint32_t agent_id;
        uint32_t owner_id;
        uint32_t model_id;
        uint16_t quantity;
        uint16_t value;
        uint32_t player_count;
        uint32_t hero_count;
        uint32_t henchman_count;
        std::wstring type;
        std::wstring rarity;
        std::wstring game_mode;
        std::wstring item_name;
        std::wstring info_string;
        std::wstring map_name;
        const wchar_t* info_string_enc;
        wchar_t map_name_enc_buf[8];
        const wchar_t* map_name_enc;
        std::wstring damage_type;
        uint16_t min_damage = 0;
        uint16_t max_damage = 0;
        std::wstring requirement_attribute;
        uint8_t requirement_value = 0;
        std::chrono::system_clock::time_point timestamp;
    };

    std::vector<PendingDrop>& GetDropHistory();
    void ClearDropHistory();
    bool IsTrackingEnabled() const;

private:
    std::vector<PendingDrop> drop_history;
};
