#pragma once

#include <GWCA/Constants/Skills.h>

#include <GWCA/Managers/UIMgr.h>

#include <Color.h>
#include <ToolboxWidget.h>

class BondsWidget : public ToolboxWidget {
    BondsWidget() = default;
    ~BondsWidget() = default;
public:
    static BondsWidget& Instance() {
        static BondsWidget instance;
        return instance;
    }

    const char* Name() const override { return "Bonds"; }
    const char* Icon() const override { return ICON_FA_BARS; }

    void Initialize() override;
    void Terminate() override;

    // Update. Will always be called every frame.
    void Update(float) override {}

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* device) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;

private:
    std::vector<GW::Constants::SkillID> skills{
        GW::Constants::SkillID::Balthazars_Spirit,
        GW::Constants::SkillID::Essence_Bond,
        GW::Constants::SkillID::Holy_Veil,
        GW::Constants::SkillID::Life_Attunement,
        GW::Constants::SkillID::Life_Barrier,
        GW::Constants::SkillID::Life_Bond,
        GW::Constants::SkillID::Live_Vicariously,
        GW::Constants::SkillID::Mending,
        GW::Constants::SkillID::Protective_Bond,
        GW::Constants::SkillID::Purifying_Veil,
        GW::Constants::SkillID::Retribution,
        GW::Constants::SkillID::Strength_of_Honor,
        GW::Constants::SkillID::Succor,
        GW::Constants::SkillID::Vital_Blessing,
        GW::Constants::SkillID::Watchful_Spirit,
        GW::Constants::SkillID::Heroic_Refrain,
        GW::Constants::SkillID::Burning_Refrain,
        GW::Constants::SkillID::Mending_Refrain,
        GW::Constants::SkillID::Bladeturn_Refrain,
        GW::Constants::SkillID::Hasty_Refrain,
        GW::Constants::SkillID::Aggressive_Refrain,
    };

    void UseBuff(GW::AgentID target, DWORD buff_skillid);

    Color background = 0;
    Color low_attribute_overlay = 0;

    std::vector<GW::Constants::SkillID> bond_list;               // index to skill id
    std::unordered_map<GW::Constants::SkillID, size_t> bond_map; // skill id to index
    bool FetchBondSkills();

    std::vector<GW::AgentID> party_list;               // index to agent id
    std::unordered_map<GW::AgentID, size_t> party_map; // agent id to index
    size_t allies_start = 255;
    bool FetchPartyInfo();

    // settings
    bool hide_in_outpost = false;
    bool click_to_cast = true;
    bool click_to_drop = true;
    bool show_allies = true;
    bool flip_bonds = false;
    int row_height = 64;

    bool snap_to_party_window = true;
    // Distance away from the party window on the x axis; used with snap to party window
    int user_offset = 64;

    GW::UI::WindowPosition* party_window_position = nullptr;
};
