#include "stdafx.h"

#include "IronManModule.h"
#include <ImGuiAddons.h>
#include <Defines.h>

#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Context/CharContext.h>
#include <Utils/ToolboxUtils.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

namespace {
    bool limit_available_skills_to_campaign = false;

    GW::Constants::Campaign GetCharacterCampaign()
    {
        const auto ctx = GW::GetCharContext();
        const auto character = ctx ? GW::AccountMgr::GetAvailableCharacter(ctx->player_name) : nullptr;
        return character ? (GW::Constants::Campaign)character->campaign() : GW::Constants::Campaign::Core;
    }

    bool IsSkillAvailable(const GW::Skill* skill) {
        if (!(skill && GW::SkillbarMgr::GetIsSkillLearnt(skill->skill_id))) 
            return false;
        if (limit_available_skills_to_campaign) {
            const auto campaign = GetCharacterCampaign();
            if (skill->campaign != campaign && skill->campaign != GW::Constants::Campaign::Core) 
                return false;
        }
        return true;
    }

    typedef GW::SkillbarSkill*(__fastcall* GetSkillbarSlot_pt)(GW::Skillbar* skillbar, void* edx, const GW::Constants::SkillID skill_id, uint32_t* skillbar_slot_out);
    GetSkillbarSlot_pt GetSkillbarSlot_Func = 0, GetSkillbarSlot_Ret = 0;

    GW::SkillbarSkill* __fastcall OnGetSkillbarSlot(GW::Skillbar* skillbar, void* edx, const GW::Constants::SkillID skill_id, uint32_t* skillbar_slot_out)
    {
        GW::Hook::EnterHook();
        if (!IsSkillAvailable(GW::SkillbarMgr::GetSkillConstantData(skill_id))) {
            GW::Hook::LeaveHook();
            return nullptr;
        }

        GW::Hook::LeaveHook();
        return GetSkillbarSlot_Ret(skillbar, edx, skill_id, skillbar_slot_out);
    }

}

void IronManModule::Update(float)
{

}

void IronManModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
    LOAD_BOOL(limit_available_skills_to_campaign);
}

void IronManModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(limit_available_skills_to_campaign);
}

void IronManModule::Terminate()
{
    ToolboxModule::Terminate();
    if (GetSkillbarSlot_Func) {
        GW::Hook::RemoveHook(GetSkillbarSlot_Func);
        GetSkillbarSlot_Func = 0;
    }
}

void IronManModule::Initialize() {
    ToolboxModule::Initialize();

    GetSkillbarSlot_Func = (GetSkillbarSlot_pt)GW::Scanner::ToFunctionStart(GW::Scanner::Find("\x69\x7b\x08\xbc\x00\x00\x00", "xxxxxxx"));
    if (GetSkillbarSlot_Func) {
        GW::Hook::CreateHook((void**)&GetSkillbarSlot_Func, OnGetSkillbarSlot, (void**)&GetSkillbarSlot_Ret);
        GW::Hook::EnableHooks(GetSkillbarSlot_Func);
    }
}

void IronManModule::DrawSettingsInternal()
{
    ImGui::Checkbox("Limit available skills to character's campaign", &limit_available_skills_to_campaign);
    ImGui::ShowHelp("e.g. if your character is from Nightfall, skill trainers and available skills will be limited to Nightfall/Core skills");
}
