#pragma once

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Constants/Skills.h>

#include <Color.h>
#include <Defines.h>
#include <ToolboxWidget.h>

class BondsWidget : public ToolboxWidget {
    static const int MAX_BONDS = 20;
    enum Bond {
        BalthazarSpirit,
        EssenceBond,
        HolyVeil,
        LifeAttunement,
        LifeBarrier,
        LifeBond,
        LiveVicariously,
        Mending,
        ProtectiveBond,
        PurifyingVeil,
        Retribution,
        StrengthOfHonor,
        Succor,
        VitalBlessing,
        WatchfulSpirit,
        HeroicRefrain,
        BurningRefrain,
        MendingRefrain,
        BladeturnRefrain,
        HastyRefrain,
        None
    };

    BondsWidget() {};
    ~BondsWidget() {};
public:
    static BondsWidget& Instance() {
        static BondsWidget instance;
        return instance;
    }

    const char* Name() const override { return "Bonds"; }

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
    void UseBuff(GW::AgentID target, DWORD buff_skillid);
    Bond GetBondBySkillID(DWORD skillid) const;

    IDirect3DTexture9* textures[MAX_BONDS];
    Color background = 0;
    Color low_attribute_overlay = 0;

    std::vector<size_t> bond_list;              // index to skill id
    std::unordered_map<DWORD, size_t> bond_map; // skill id to index
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
    int row_height = 0;
};
