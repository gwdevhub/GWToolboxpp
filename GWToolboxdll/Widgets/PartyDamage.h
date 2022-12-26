#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>

#include <ToolboxWidget.h>

class PartyDamage : public ToolboxWidget {
    PartyDamage() = default;
    ~PartyDamage() = default;

    static constexpr size_t MAX_PLAYERS = 12;

    struct PlayerDamage {
        uint32_t damage = 0;
        uint32_t recent_damage = 0;
        clock_t last_damage = 0;
        uint32_t agent_id = 0;
        std::wstring name;
        GW::Constants::Profession primary = GW::Constants::Profession::None;
        GW::Constants::Profession secondary = GW::Constants::Profession::None;

        void Reset() {
            damage = 0;
            recent_damage = 0;
            agent_id = 0;
            name = L"";
            primary = GW::Constants::Profession::None;
            secondary = GW::Constants::Profession::None;
        }
    };

public:
    static PartyDamage& Instance() {
        static PartyDamage instance;
        return instance;
    }

    const char* Name() const override { return "Damage"; }
    const char* Icon() const override { return ICON_FA_BARS; }

    void Initialize() override;
    void Terminate() override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void Update(float delta) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;

    void WritePartyDamage();
    void WriteDamageOf(const size_t index, uint32_t rank = 0); // party index from 0 to 12
    void WriteOwnDamage();
    void ResetDamage();

private:
    void DamagePacketCallback(GW::HookStatus *, GW::Packet::StoC::GenericModifier *packet);
    void MapLoadedCallback(GW::HookStatus *, GW::Packet::StoC::MapLoaded *packet);

    void CreatePartyIndexMap();

    float GetPartOfTotal(uint32_t dmg) const;
    float GetPercentageOfTotal(const uint32_t dmg) const { return GetPartOfTotal(dmg) * 100.0f; }

    // damage values
    uint32_t total = 0;
    PlayerDamage damage[MAX_PLAYERS];
    std::map<DWORD, uint32_t> hp_map;
    std::map<DWORD, size_t> party_index;
    size_t player_index = 0;
    GW::UI::WindowPosition* party_window_position = nullptr;

    // main routine variables
    bool in_explorable = false;
    clock_t send_timer = 0;
    std::queue<std::wstring> send_queue;

    // ini
    ToolboxIni* inifile = nullptr;
    Color color_background = 0;
    Color color_damage = 0;
    Color color_recent = 0;
    float width = 0.f;
    bool bars_left = false;
    int recent_max_time = 0;
    int row_height = 0;
    bool hide_in_outpost = false;
    bool print_by_click = false;

    bool snap_to_party_window = true;
    // Distance away from the party window on the x axis; used with snap to party window
    int user_offset = 0;

    GW::HookEntry GenericModifier_Entry;
    GW::HookEntry MapLoaded_Entry;
};
