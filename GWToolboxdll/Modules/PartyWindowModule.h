#pragma once

#include <ToolboxModule.h>
#include <GWCA/Packets/StoC.h>

class PartyWindowModule : public ToolboxModule {
    PartyWindowModule() = default;

public:
    static PartyWindowModule& Instance()
    {
        static PartyWindowModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Party Window"; }
    [[nodiscard]] const char* Description() const override { return "Includes features to:\nAdd player numbers to party window in explorable area\nAbility to add other NPCs to the party window"; }
    [[nodiscard]] const char* SettingsName() const override { return "Party Settings"; }

    struct Settings {
        bool add_npcs_to_party_window = true; // Quick tickbox to disable the module without restarting TB
        bool add_player_numbers_to_party_window = false;
        bool add_elite_skill_to_summons = false;
        bool remove_dead_imperials = false;
        bool custom_sort_party_window = false;
    };

    struct CustomNPC {
        std::string alias;
        uint32_t model_id = 0;
        uint32_t map_id = 0;
    };

    struct PartySortingSetting {
        uint32_t map_id = 0;
        uint32_t party_size = 0;
        std::vector<uint16_t> sorting_by_profession;
    };

    void Initialize() override;
    void Terminate() override;
    void Update(float) override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;
    const std::map<std::wstring, std::wstring>& GetAliasedPlayerNames();

private:
    static void LoadDefaults();
    std::map<std::wstring, std::wstring> aliased_player_names{};
};
