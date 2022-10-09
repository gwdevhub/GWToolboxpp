#pragma once

#include <GWCA\GameEntities\Skill.h>
#include <ToolboxWindow.h>

/*namespace {
    enum SkillTypesEncoded {
        Attack = 0x48f, EliteAttack,
        AxeAttack, EliteAxeAttack,
        BowAttack, EliteBowAttack,
        HammerAttack, EliteHammerAttack,
        MeleeAttack, EliteMeleeAttack,
        SwordAttack, EliteSwordAttack,
        Preparation, ElitePreparation,
        DualAttack, EliteDualAttack,
        LeadAttack, EliteLeadAttack,
        OffHandAttack, EliteOffHandAttack,
        PartyBonus,
        NatureRitual, EliteNatureRitual,
        BindingRitual, EliteBindingRitual,
        Signet, EliteSignet,
        Shout, EliteShout,
        Spell, EliteSpell,
        HexSpell, EliteHexSpell,
        EnchantmentSpell, EliteEnchantmentSpell,
        Glyph, EliteGlyph,
        Skill, EliteSkill
    };
}*/
class SkillListingWindow : public ToolboxWindow {
public:
    class Skill {
    public:
        Skill(GW::Skill* _gw_skill) : skill(_gw_skill) {}
        nlohmann::json ToJson();
        const wchar_t* Name();
        const wchar_t* GWWDescription();
        const wchar_t* GWWConcise();
        //const wchar_t* Description(uint32_t attribute_level, wchar_t* buffer);
        //const wchar_t* Concise(uint32_t attribute_level, wchar_t* buffer);
        GW::Skill* skill;
        const std::wstring GetSkillType();
        const bool HasExhaustion() { return skill->special & 0x1; }
        const bool IsMaintained() { return skill->duration0 == 0x20000;  }
        const bool IsPvE() { return (skill->special & 0x80000) != 0;  }
        const bool IsElite() { return (skill->special & 0x4) != 0; }
    protected:
        const wchar_t *Description();
        const wchar_t *Concise();
    private:
        wchar_t name_enc[64] = { 0 };
        wchar_t name_dec[256] = { 0 };
        wchar_t desc_enc[64] = { 0 };
        wchar_t desc_dec[256] = { 0 };
        wchar_t desc_gww[256] = { 0 };
        wchar_t concise_enc[64] = { 0 };
        wchar_t concise_dec[256] = { 0 };
        wchar_t concise_gww[256] = { 0 };
    };
private:
    SkillListingWindow() = default;
    std::vector<Skill*> skills{};
public:
    static SkillListingWindow& Instance() {
        static SkillListingWindow instance;
        return instance;
    }

    const char* Name() const override { return "Guild Wars Skill List"; }
    const char* Icon() const override { return ICON_FA_LIST; }

    void Draw(IDirect3DDevice9* pDevice) override;
    void Initialize() override;
    void Terminate() override;

    void ExportToJSON();
};
