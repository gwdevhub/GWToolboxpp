#pragma once

#include <Defines.h>

#include <GWCA/Packets/StoC.h>

#include <Widgets/SnapsToPartyWindow.h>

namespace GW {
    namespace Packet {
        namespace StoC {
            struct MapLoaded;
        }
    }
    struct HookStatus;
}

class PartyDamage : public SnapsToPartyWindow {
protected:
    struct PlayerDamage;
    static std::vector<PartyDamage::PlayerDamage> damage;
    static std::vector<PartyDamage::PlayerDamage> departed_damage;
    static std::vector<uint32_t> prev_party_agent_ids;

    static void ReconcileDamageIndices();
    static void WriteDamageOf(size_t index, uint32_t rank = 0);
    static void WritePartyDamage();
    static void WriteOwnDamage();
    static void ResetDamage();

    static PlayerDamage* GetDamageByAgentId(uint32_t agent_id, uint32_t* party_index_out = nullptr);

    static void CHAT_CMD_FUNC(CmdDamage);

    static void MapLoadedCallback(GW::HookStatus*, const GW::Packet::StoC::MapLoaded*);
    static void DamagePacketCallback(GW::HookStatus*, const GW::Packet::StoC::GenericModifier*);
    static void ConditionValueCallback(GW::HookStatus*, const GW::Packet::StoC::GenericValue*);

public:
    static PartyDamage& Instance()
    {
        static PartyDamage instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Damage"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_BARS; }

    struct Settings {
        Colors::SettingColor color_background = Colors::ARGB(76, 0, 0, 0);
        Colors::SettingColor color_damage = Colors::ARGB(76, 0, 0, 0);
        Colors::SettingColor color_recent = Colors::ARGB(205, 102, 153, 230);
        Colors::SettingColor color_healing = Colors::ARGB(205, 102, 230, 102);
        float width = 100.0f;
        bool bars_left = true;
        int recent_max_time = 7000;
        bool hide_in_outpost = false;
        bool print_by_click = false;
        bool overlay_party_window = false;
        bool show_damage = true;
        bool show_healing = false;
        bool show_dps = false;
        bool show_condition_dps = false;
        // Distance away from the party window on the x axis; used with snap to party window
        int user_offset = 0;
    };

    void Initialize() override;
    void Terminate() override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void Update(float delta) override;

    static DWORD GetMaxHp(DWORD player_number);

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;

};