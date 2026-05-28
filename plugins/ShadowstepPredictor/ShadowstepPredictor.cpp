#include "ShadowstepPredictor.h"

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Packets/StoC.h>

namespace
{
    GW::HookEntry skillCastEntry;
    GW::HookEntry genericValueEntry;
    GW::HookEntry instanceLoadEntry;

    const GW::PathingMapArray* path_map = nullptr;
} // namespace

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static ShadowstepPredictor instance;
    return &instance;
}

void ShadowstepPredictor::Update(float delta)
{
    ToolboxUIPlugin::Update(delta);

    const auto player = GW::Agents::GetControlledCharacter();
    const auto target = GW::Agents::GetTarget();

    if (!player || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable)
    {
        chances.clear();
        return;
    }
    if (!path_map) path_map = GW::Map::GetPathingMap();
    if (!path_map) 
    {
        chances.clear();
        return;
    }

    const auto playerPathPoint = PathPoint{player->pos, findTrapezoid(player->pos, path_map)};

    // ------ Effects ------
    const auto checkEffect = [&](GW::Constants::SkillID id, const PathPoint& p) 
    {
        if (GW::Effects::GetPlayerEffectBySkillId(id)) 
        {
            chances[id] = sohPrediction(playerPathPoint, p);
        }
        else 
        {
            chances.erase(id);
        }
    };
    if (showSoH)
        checkEffect(GW::Constants::SkillID::Shadow_of_Haste, shadowOfHasteLocation);
    if (showShadowWalk)
        checkEffect(GW::Constants::SkillID::Shadow_Walk, shadowWalkLocation);
    if (showRecall && GW::Effects::GetPlayerEffectBySkillId(GW::Constants::SkillID::Recall))
    {
        chances[GW::Constants::SkillID::Recall] = dcPrediction(playerPathPoint, GW::Agents::GetAgentByID(recallTargetId), path_map);
    }
    else
    {
        chances.erase(GW::Constants::SkillID::Recall);
    }

    // ------ Ally Jumps ------
    std::optional<OutcomeChances> allyJumpChances;
    const auto checkAllyJump = [&](GW::Constants::SkillID id) 
    {
        const auto canAllyJump = target && target->GetIsLivingType() && target->GetAsAgentLiving()->allegiance != GW::Constants::Allegiance::Enemy && GW::GetSquareDistance(player->pos, target->pos) < GW::Constants::SqrRange::Spellcast;
        if (!canAllyJump) 
        {
            chances.erase(id);
            return;
        }
        const auto slot = GW::SkillbarMgr::GetSkillSlot(id);
        if (slot == -1) 
        {
            chances.erase(id);
            return;
        }

        if (allyJumpChances)
            chances[id] = *allyJumpChances;
        else {
            allyJumpChances = chances[id] = dcPrediction(playerPathPoint, target, path_map);
        }
    };

    if (showAllyShadowSteps) 
    {
        checkAllyJump(GW::Constants::SkillID::Ebon_Escape);
        if (!showRecall || !GW::Effects::GetPlayerEffectBySkillId(GW::Constants::SkillID::Recall)) 
            checkAllyJump(GW::Constants::SkillID::Recall);
        checkAllyJump(GW::Constants::SkillID::Deaths_Retreat);
        checkAllyJump(GW::Constants::SkillID::Return);
    }

    // ------ Enemy Jumps ------
    std::optional<OutcomeChances> enemyJumpChances;
    const auto checkEnemyJump = [&](GW::Constants::SkillID id) 
    {
        const auto canEnemyJump = target && target->GetIsLivingType() && target->GetAsAgentLiving()->allegiance == GW::Constants::Allegiance::Enemy && GW::GetSquareDistance(player->pos, target->pos) < GW::Constants::SqrRange::Spellcast;
        if (!canEnemyJump) 
        {
            chances.erase(id);
            return;
        }
        const auto slot = GW::SkillbarMgr::GetSkillSlot(id);
        if (slot == -1) 
        {
            chances.erase(id);
            return;
        }

        if (enemyJumpChances)
            chances[id] = *enemyJumpChances;
        else
            enemyJumpChances = chances[id] = dcPrediction(playerPathPoint, target, path_map);
    };

    if (showEnemyShadowSteps) 
    {
        checkEnemyJump(GW::Constants::SkillID::Deaths_Charge);
        checkEnemyJump(GW::Constants::SkillID::Dark_Prison);
        checkEnemyJump(GW::Constants::SkillID::Scorpion_Wire);
        checkEnemyJump(GW::Constants::SkillID::Shadow_Fang);
        checkEnemyJump(GW::Constants::SkillID::Shadow_Prison);
        checkEnemyJump(GW::Constants::SkillID::Shadow_Theft);
        checkEnemyJump(GW::Constants::SkillID::Wastrels_Collapse);
        if (!GW::Effects::GetPlayerEffectBySkillId(GW::Constants::SkillID::Shadow_Walk)) 
            checkEnemyJump(GW::Constants::SkillID::Shadow_Walk);
    }
}

namespace 
{
    void drawOverlay(ImVec2 topLeft, ImVec2 size, OutcomeChances chances, ImVec4 success, ImVec4 partial, ImVec4 failure) 
    {
        const auto draw_list = ImGui::GetForegroundDrawList();

        const auto successStart = topLeft.x;
        const auto partialStart = successStart + chances.success * size.x;
        const auto failureStart = partialStart + chances.partial * size.x;
        const auto failureEnd = partialStart + chances.failure * size.x;

        if (chances.success > 0.)
            draw_list->AddRectFilled({successStart, topLeft.y}, {partialStart, topLeft.y + size.y}, ImGui::ColorConvertFloat4ToU32(success));
        if (chances.partial > 0.)
            draw_list->AddRectFilled({partialStart, topLeft.y}, {failureStart, topLeft.y + size.y}, ImGui::ColorConvertFloat4ToU32(partial));
        if (chances.failure > 0.)
            draw_list->AddRectFilled({failureStart, topLeft.y}, {  failureEnd, topLeft.y + size.y}, ImGui::ColorConvertFloat4ToU32(failure));
    }
}

void ShadowstepPredictor::Draw(IDirect3DDevice9*) 
{
    if (!GetVisiblePtr() || !*GetVisiblePtr())
        return;
    const auto skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!skillbar) return;

    auto* skillbar_frame = GW::UI::GetFrameByLabel(L"Skillbar");
    if (!skillbar_frame || !skillbar_frame->IsVisible() || !skillbar_frame->IsCreated())
        return;

    const auto imgui_viewport = ImGui::GetMainViewport();

    for (size_t i = 0; i < 8; i++) 
    {
        if (!chances.contains(skillbar->skills[i].skill_id))
            continue;

        auto* skill_frame = GW::UI::GetChildFrame(skillbar_frame, i);
        if (!skill_frame) continue;

        const auto screen_pos = skill_frame->position.GetTopLeftOnScreen();
        const auto size = skill_frame->position.GetSizeOnScreen();
        const ImVec2 topLeft{screen_pos.x + imgui_viewport->Pos.x, screen_pos.y + imgui_viewport->Pos.y};

        drawOverlay(topLeft, {size.x, size.y}, chances[skillbar->skills[i].skill_id], successColor, partialColor, failureColor);
    }
}

void ShadowstepPredictor::LoadSettings(const wchar_t* folder)
{
    ToolboxUIPlugin::LoadSettings(folder);
    
    const auto ini = LoadIni(folder);

    const auto loadColor = [&](ImVec4& color, std::string varName) 
    {
        color.x = (float)ini.GetDoubleValue(Name(), (varName + "x").c_str(), color.x);
        color.y = (float)ini.GetDoubleValue(Name(), (varName + "y").c_str(), color.y);
        color.z = (float)ini.GetDoubleValue(Name(), (varName + "z").c_str(), color.z);
        color.w = (float)ini.GetDoubleValue(Name(), (varName + "w").c_str(), color.w);
    };
    loadColor(successColor, VAR_NAME(successColor));
    loadColor(partialColor, VAR_NAME(partialColor));
    loadColor(failureColor, VAR_NAME(failureColor));
    
    showRecall           = ini.GetBoolValue(Name(), VAR_NAME(showRecall),           showRecall);
    showSoH              = ini.GetBoolValue(Name(), VAR_NAME(showSoH),              showSoH);
    showShadowWalk       = ini.GetBoolValue(Name(), VAR_NAME(showShadowWalk),       showShadowWalk);
    showAllyShadowSteps  = ini.GetBoolValue(Name(), VAR_NAME(showAllyShadowSteps),  showAllyShadowSteps);
    showEnemyShadowSteps = ini.GetBoolValue(Name(), VAR_NAME(showEnemyShadowSteps), showEnemyShadowSteps);
}

void ShadowstepPredictor::SaveSettings(const wchar_t* folder)
{
    ToolboxUIPlugin::SaveSettings(folder);

    auto ini = LoadIni(folder);
    
    const auto saveColor = [&](const ImVec4& color, std::string varName) 
    {
        ini.SetDoubleValue(Name(), (varName + "x").c_str(), color.x);
        ini.SetDoubleValue(Name(), (varName + "y").c_str(), color.y);
        ini.SetDoubleValue(Name(), (varName + "z").c_str(), color.z);
        ini.SetDoubleValue(Name(), (varName + "w").c_str(), color.w);
    };
    saveColor(successColor, VAR_NAME(successColor));
    saveColor(partialColor, VAR_NAME(partialColor));
    saveColor(failureColor, VAR_NAME(failureColor));
    
    ini.SetBoolValue(Name(), VAR_NAME(showRecall),           showRecall);
    ini.SetBoolValue(Name(), VAR_NAME(showSoH),              showSoH);
    ini.SetBoolValue(Name(), VAR_NAME(showShadowWalk),       showShadowWalk);
    ini.SetBoolValue(Name(), VAR_NAME(showAllyShadowSteps),  showAllyShadowSteps);
    ini.SetBoolValue(Name(), VAR_NAME(showEnemyShadowSteps), showEnemyShadowSteps);

    PLUGIN_ASSERT(ini.SaveFile(ini.location_on_disk) == SI_OK);
}

void ShadowstepPredictor::DrawSettings()
{
    if (!toolbox_handle) {
        return;
    }
    ToolboxUIPlugin::DrawSettings();

    bool changed = false;

    ImGui::Text("Show:");
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Recall", &showRecall);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Shadow of Haste", &showSoH);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Shadow Walk", &showShadowWalk);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Ally Shadow Steps", &showAllyShadowSteps);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Enemy Shadow Steps", &showEnemyShadowSteps);

    if (changed) chances.clear();

    ImGui::ColorEdit4("Success", reinterpret_cast<float*>(&successColor.x));
    ImGui::ColorEdit4("Partial", reinterpret_cast<float*>(&partialColor.x));
    ImGui::ColorEdit4("Failure", reinterpret_cast<float*>(&failureColor.x));

    ImGui::Text("Version 1.0.5");
}

void ShadowstepPredictor::Initialize(ImGuiContext* ctx, const ImGuiAllocFns allocator_fns, const HMODULE toolbox_dll)
{
    ToolboxUIPlugin::Initialize(ctx, allocator_fns, toolbox_dll);
    InitializePathing();

    GW::UI::RegisterUIMessageCallback(&skillCastEntry, GW::UI::UIMessage::kSkillActivated, [this](GW::HookStatus*, const GW::UI::UIMessage, void* wParam, void*) 
    {
        struct Payload {
            uint32_t agent_id;
            GW::Constants::SkillID skill_id;
        };
        const auto payload = *static_cast<Payload*>(wParam);
        const auto player = GW::Agents::GetControlledCharacter();
        if (!player || payload.agent_id != player->agent_id) return;

        switch (payload.skill_id) 
        {
            case GW::Constants::SkillID::Shadow_of_Haste:
                shadowOfHasteLocation = {player->pos, findTrapezoid(player->pos, path_map)};
                break;
            case GW::Constants::SkillID::Shadow_Walk:
                shadowWalkLocation = {player->pos, findTrapezoid(player->pos, path_map)};
                break;
            default:
                return;
        }
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadFile>(&instanceLoadEntry, [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadFile*) {
        chances.clear();
        shadowOfHasteLocation = {};
        shadowWalkLocation = {};
        recallTargetId = 0;
        path_map = nullptr;
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(&genericValueEntry, [&](GW::HookStatus*, const GW::Packet::StoC::GenericValueTarget* pak) {
        if (pak->Value_id == 20 && pak->caster == GW::Agents::GetControlledCharacterId() && pak->value == 928) 
        {
            recallTargetId = pak->target;
        }
    });
}

void ShadowstepPredictor::SignalTerminate()
{
    ToolboxUIPlugin::SignalTerminate();
    
    GW::StoC::RemoveCallbacks(&instanceLoadEntry);
    GW::StoC::RemoveCallbacks(&genericValueEntry);
    GW::UI::RemoveUIMessageCallback(&skillCastEntry, GW::UI::UIMessage::kSkillActivated);
}
