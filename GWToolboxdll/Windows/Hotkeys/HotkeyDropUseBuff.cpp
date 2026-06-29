#include "stdafx.h"

#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <ImGuiAddons.h>

#include <Windows/Hotkeys/HotkeyDropUseBuff.h>

bool HotkeyDropUseBuff::GetText(void* data, int idx, const char** out_text)
{
    static char other_buf[64];
    switch (static_cast<SkillIndex>(idx)) {
        case Recall:
            *out_text = "Recall";
            break;
        case UA:
            *out_text = "UA";
            break;
        case HolyVeil:
            *out_text = "Holy Veil";
            break;
        default:
            snprintf(other_buf, 64, "Skill#%d", (int)data);
            *out_text = other_buf;
            break;
    }
    return true;
}

HotkeyDropUseBuff::SkillIndex HotkeyDropUseBuff::GetIndex() const
{
    switch (id) {
        case GW::Constants::SkillID::Recall:
            return Recall;
        case GW::Constants::SkillID::Unyielding_Aura:
            return UA;
        case GW::Constants::SkillID::Holy_Veil:
            return HolyVeil;
        default:
            return Other;
    }
}

HotkeyDropUseBuff::HotkeyDropUseBuff(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    id = static_cast<GW::Constants::SkillID>(ini ? ini->GetLongValue(
        section, "SkillID", static_cast<long>(GW::Constants::SkillID::Recall)) : static_cast<long>(GW::Constants::SkillID::Recall));
}

void HotkeyDropUseBuff::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "SkillID", static_cast<long>(id));
}

int HotkeyDropUseBuff::Description(char* buf, const size_t bufsz)
{
    const char* skillname;
    GetText((void*)id, GetIndex(), &skillname);
    return snprintf(buf, bufsz, "Drop/Use %s", skillname);
}

bool HotkeyDropUseBuff::DrawSettings()
{
    bool hotkey_changed = false;
    SkillIndex index = GetIndex();
    if (ImGui::Combo("Skill", (int*)&index,
                     "Recall\0Unyielding Aura\0Holy Veil\0Other", 4)) {
        switch (index) {
            case Recall:
                id = GW::Constants::SkillID::Recall;
                break;
            case UA:
                id = GW::Constants::SkillID::Unyielding_Aura;
                break;
            case HolyVeil:
                id = GW::Constants::SkillID::Holy_Veil;
                break;
            case Other:
                id = static_cast<GW::Constants::SkillID>(0);
                break;
            default:
                break;
        }
        hotkey_changed = true;
    }
    if (index == Other) {
        if (ImGui::InputInt("Skill ID", (int*)&id)) {
            hotkey_changed = true;
        }
    }
    hotkey_changed = hotkey_changed || TBHotkey::DrawSettings();
    return hotkey_changed;
}

void HotkeyDropUseBuff::Execute()
{
    if (!CanUse()) {
        return;
    }

    const GW::Buff* buff = GW::Effects::GetPlayerBuffBySkillId(id);
    if (buff) {
        GW::Effects::DropBuff(buff->buff_id);
    }
    else {
        const int islot = GW::SkillbarMgr::GetSkillSlot(id);
        if (islot >= 0) {
            uint32_t slot = static_cast<uint32_t>(islot);
            if (GW::SkillbarMgr::GetPlayerSkillbar()->skills[slot].recharge == 0) {
                GW::GameThread::Enqueue([slot] {
                    GW::SkillbarMgr::UseSkill(slot);
                });
            }
        }
    }
}
