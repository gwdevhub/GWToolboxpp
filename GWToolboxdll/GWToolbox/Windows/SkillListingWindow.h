#pragma once

#include "ToolboxWindow.h"

#include <vector>
#include <queue>
#include <string>
#include <GWCA\Constants\Constants.h>

#include <GWCA\GameContainers\Array.h>

#include <GWCA\GameEntities\Skill.h>
#include "GWCA\Managers\UIMgr.h"

#include "Timer.h"
#include "GuiUtils.h"
namespace {
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
}
class SkillListingWindow : public ToolboxWindow {
public:
    class Skill {
    public:
        Skill(GW::Skill* _gw_skill) : skill(_gw_skill) {

        }
		nlohmann::json ToJson() {
			nlohmann::json json;
			json["n"] = GuiUtils::WStringToString(Name());
			json["d"] = GuiUtils::WStringToString(GWWDescription());
			json["cd"] = GuiUtils::WStringToString(GWWConcise());
			json["t"] = skill->type;
			json["p"] = skill->profession;
			json["a"] = IsPvE() ? 255 - skill->h002A[0] : skill->attribute;
			if (IsElite())
				json["e"] = 1;
			json["c"] = skill->campaign;
			nlohmann::json z_json;
			if (HasExhaustion())     z_json["x"] = skill->h0034;
			if (skill->recharge)     z_json["r"] = skill->recharge;
			if (skill->activation)   z_json["c"] = skill->activation;
			if (IsMaintained())		z_json["d"] = 1;
			if (skill->adrenaline)   z_json["a"] = skill->adrenaline;
			if (skill->energy_cost)  z_json["e"] = skill->GetEnergyCost();
			if (skill->health_cost)  z_json["s"] = skill->health_cost;
			if (z_json.size())
				json["z"] = z_json;
			return json;
		};
        const wchar_t* Name();
        const wchar_t* GWWDescription();
        const wchar_t* GWWConcise();
        //const wchar_t* Description(uint32_t attribute_level, wchar_t* buffer);
        //const wchar_t* Concise(uint32_t attribute_level, wchar_t* buffer);
        GW::Skill* skill;
		const std::wstring GetSkillType() {
			std::wstring str(IsElite() ? L"Elite " : L"");
			switch (skill->type) {
			case 3:
				return str += L"Stance", str;
			case 4:
				return str += L"Hex Spell", str;
			case 5:
				return str += L"Spell", str;
			case 6:
				if(skill->special & 0x800000)
					str += L"Flash ";
				return str += L"Enchantment Spell", str;
			case 7:
				return str += L"Signet", str;
			case 9:
				return str += L"Well Spell", str;
			case 10:
				return str += L"Touch Skill", str;
			case 11:
				return str += L"Ward Spell", str;
			case 12:
				return str += L"Glyph", str;
			case 14:
				switch (skill->weapon_req) {
				case 1:
					return str += L"Axe Attack", str;
				case 2:
					return str += L"Bow Attack", str;
				case 8:
					switch (skill->combo) {
					case 1: return str += L"Lead Attack", str;
					case 2: return str += L"Off-Hand Attack", str;
					case 3: return str += L"Dual Attack", str;
					}
					return str += L"Dagger Attack", str;
				case 16:
					return str += L"Hammer Attack", str;
				case 32:
					return str += L"Scythe Attack", str;
				case 64:
					return str += L"Spear Attack", str;
				case 70:
					return str += L"Ranged Attack", str;
				case 128:
					return str += L"Sword Attack", str;
				}
				return str += L"Melee Attack", str;
			case 15:
				return str += L"Shout", str;
			case 19:
				return str += L"Preparation", str;
			case 20:
				return str += L"Pet Attack", str;
			case 21:
				return str += L"Trap", str;
			case 22:
				switch (skill->profession) {
				case 8:
					return  str += L"Binding Ritual", str;
				case 2:
					return  str += L"Nature Ritual", str;
				}
				return str += L"Ebon Vanguard Ritual", str;
			case 24:
				return str += L"Item Spell", str;
			case 25:
				return str += L"Weapon Spell", str;
			case 26:
				return str += L"Form", str;
			case 27:
				return str += L"Chant", str;
			case 28:
				return str += L"Echo", str;
			default:
				return str += L"Skill", str;
			}
		};
        const bool HasExhaustion() { return skill->special & 0x1; }
		const bool IsMaintained() { return skill->duration0 == 0x20000;  }
		const bool IsPvE() { return skill->special & 0x80000;  }
        const bool IsElite() { return skill->special & 0x4; }
    protected:
        const wchar_t* Description() {
            if (!desc_enc[0] && GW::UI::UInt32ToEncStr(skill->description, desc_enc, 16)) {
				wchar_t buf[64] = { 0 };
                swprintf(buf, 64, L"%s\x10A\x104\x101%c\x1\x10B\x104\x101%c\x1\x10C\x104\x101%c\x1", desc_enc,
                    0x100 + (skill->scale0 == skill->scale15 ? skill->scale0 : 991),
                    0x100 + (skill->bonusScale0 == skill->bonusScale15 ? skill->bonusScale0 : 992),
                    0x100 + (skill->duration0 == skill->duration15 ? skill->duration0 : 993));
				wcscpy(desc_enc, buf);
                GW::UI::AsyncDecodeStr(desc_enc, desc_dec, 256);
            }
            return desc_dec;
        }
        const wchar_t* Concise() {
            if (!concise_enc[0] && GW::UI::UInt32ToEncStr(skill->h0098, concise_enc, 16)) {
				wchar_t buf[64] = { 0 };
                swprintf(buf, 64, L"%s\x10A\x104\x101%c\x1\x10B\x104\x101%c\x1\x10C\x104\x101%c\x1", concise_enc,
					0x100 + (skill->scale0 == skill->scale15 ? skill->scale0 : 991),
					0x100 + (skill->bonusScale0 == skill->bonusScale15 ? skill->bonusScale0 : 992),
					0x100 + (skill->duration0 == skill->duration15 ? skill->duration0 : 993));
				wcscpy(concise_enc, buf);
                GW::UI::AsyncDecodeStr(concise_enc, concise_dec, 256);
            }
            return concise_dec;
        }
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
    SkillListingWindow() {};
    ~SkillListingWindow() {
        for (unsigned int i = 0; i < skills.size(); i++) {
            if(skills[i]) delete skills[i];
        }
        skills.clear();
    };

    std::vector<Skill* > skills;
public:
    static SkillListingWindow& Instance() {
        static SkillListingWindow instance;
        return instance;
    }
    void ExportToJSON();
    const char* Name() const override { return "Guild Wars Skill List"; }
    void Draw(IDirect3DDevice9* pDevice) override;
    void Initialize() override;

};
