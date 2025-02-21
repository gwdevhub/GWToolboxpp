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

    static void WriteDamageOf(size_t index, uint32_t rank = 0);
    static void WritePartyDamage();
    static void WriteOwnDamage();
    static void ResetDamage();

    static PlayerDamage* GetDamageByAgentId(uint32_t agent_id, uint32_t* party_index_out = nullptr);

    static void CHAT_CMD_FUNC(CmdDamage);

    static void MapLoadedCallback(GW::HookStatus*, const GW::Packet::StoC::MapLoaded*);
    static void DamagePacketCallback(GW::HookStatus*, const GW::Packet::StoC::GenericModifier*);

public:
    static PartyDamage& Instance()
    {
        static PartyDamage instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Damage"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_BARS; }

    void Initialize() override;
    void Terminate() override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void Update(float delta) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

};
