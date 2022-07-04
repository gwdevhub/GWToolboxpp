#pragma once

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Constants/Skills.h>

#include <GWCA/Managers/UIMgr.h>

#include <Color.h>
#include <Defines.h>
#include <ToolboxWidget.h>

using namespace GW::Constants;

class BondsWidget : public ToolboxWidget {
    BondsWidget() {};
    ~BondsWidget() {};
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

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;

private:
    std::vector<SkillID> skills{
        SkillID::Balthazars_Spirit,
        SkillID::Essence_Bond,
        SkillID::Holy_Veil,
        SkillID::Life_Attunement,
        SkillID::Life_Barrier,
        SkillID::Life_Bond,
        SkillID::Live_Vicariously,
        SkillID::Mending,
        SkillID::Protective_Bond,
        SkillID::Purifying_Veil,
        SkillID::Retribution,
        SkillID::Strength_of_Honor,
        SkillID::Succor,
        SkillID::Vital_Blessing,
        SkillID::Watchful_Spirit,
        SkillID::Heroic_Refrain,
        SkillID::Burning_Refrain,
        SkillID::Mending_Refrain,
        SkillID::Bladeturn_Refrain,
        SkillID::Hasty_Refrain,
    };

    void UseBuff(GW::AgentID target, DWORD buff_skillid);

    Color background = 0;
    Color low_attribute_overlay = 0;

    std::vector<SkillID> bond_list;               // index to skill id
    std::unordered_map<SkillID, size_t> bond_map; // skill id to index
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
