#include "SpeedrunScriptingTools.h"

#include <ConditionImpls.h>
#include <ActionImpls.h>

#include <GWCA/GWCA.h>

#include <GWCA/Utilities/Hooker.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Constants/Constants.h>

#include <Keys.h>

#include <imgui.h>
#include <SimpleIni.h>
#include <filesystem>

namespace {
    std::string_view toString(ConditionType type) {
        switch (type) {
            case ConditionType::IsInMap:
                return "Current map ID";
            case ConditionType::QuestHasState:
                return "Queststate";
            case ConditionType::PartyPlayerCount:
                return "Party player count";
            case ConditionType::InstanceProgress:
                return "Instance progress";
            case ConditionType::HasPartyWindowAllyOfName:
                return "Party window contains ally";

            case ConditionType::PlayerIsNearPosition:
                return "Player position";
            case ConditionType::PlayerHasBuff:
                return "Player buff";
            case ConditionType::PlayerHasSkill:
                return "Player has skill";

            case ConditionType::CurrentTargetHasHpPercentBelow:
                return "Current target HP";
            case ConditionType::CurrentTargetIsUsingSkill:
                return "Current target skill";

            case ConditionType::NearbyAllyOfModelIdExists:
                return "Has nearby ally with model id";
            case ConditionType::NearbyEnemyWithModelIdCastingSpellExists:
                return "Nearby enemy casts skill";
            case ConditionType::EnemyWithModelIdAndEffectExists:
                return "Enemy has effect";
            case ConditionType::EnemyInPolygonWithModelIdExists:
                return "Enemies in polygon";
            case ConditionType::EnemyInCircleSegmentWithModelIdExísts:
                return "Enemies in circle segment";
            default:
                return "Unknown";
        }
    }
    std::shared_ptr<Condition> makeCondition(ConditionType type) {
        switch (type) {
            case ConditionType::IsInMap:
                return std::make_shared<IsInMapCondition>();
            case ConditionType::QuestHasState:
                return nullptr;
            case ConditionType::PartyPlayerCount:
                return std::make_shared<PartyPlayerCountCondition>();
            case ConditionType::InstanceProgress:
                return std::make_shared<InstanceProgressCondition>();
            case ConditionType::HasPartyWindowAllyOfName:
                return nullptr;

            case ConditionType::PlayerIsNearPosition:
                return std::make_shared<PlayerIsNearPositionCondition>();
            case ConditionType::PlayerHasBuff:
                return std::make_shared<PlayerHasBuffCondition>();
            case ConditionType::PlayerHasSkill:
                return std::make_shared<PlayerHasSkillCondition>();

            case ConditionType::CurrentTargetHasHpPercentBelow:
                return nullptr;
            case ConditionType::CurrentTargetIsUsingSkill:
                return std::make_shared<CurrentTargetIsCastingSkillCondition>();

            case ConditionType::NearbyAllyOfModelIdExists:
                return nullptr;
            case ConditionType::NearbyEnemyWithModelIdCastingSpellExists:
                return nullptr;
            case ConditionType::EnemyWithModelIdAndEffectExists:
                return nullptr;
            case ConditionType::EnemyInPolygonWithModelIdExists:
                return nullptr;
            case ConditionType::EnemyInCircleSegmentWithModelIdExísts:
                return nullptr;
            default:
                return nullptr;
        }
    }
    std::string_view toString(ActionType type)
    {
        switch (type) {
            case ActionType::MoveTo: 
                return "Move to";
            case ActionType::CastOnSelf:
                return "Cast on self";
            case ActionType::CastOnTarget:
                return "Cast on current target";
            case ActionType::UseItem:
                return "Use item";
            case ActionType::SendDialog:
                return "Send dialog";
            case ActionType::GoToNpc:
                return "Go to NPC";
            case ActionType::Wait:
                return "Wait";
            case ActionType::SendChat:
                return "Send chat";
            default:
                return "Unknown";
        }
    }
    std::shared_ptr<Action> makeAction(ActionType type)
    {
        switch (type) {
            case ActionType::MoveTo:
                return std::make_shared<MoveToAction>();
            case ActionType::CastOnSelf:
                return std::make_shared<CastOnSelfAction>();
            case ActionType::CastOnTarget:
                return std::make_shared<CastOnTargetAction>();
            case ActionType::UseItem:
                return std::make_shared<UseItemAction>();
            case ActionType::SendDialog:
                return std::make_shared<SendDialogAction>();
            case ActionType::GoToNpc:
                return std::make_shared<GoToNpcAction>();
            case ActionType::Wait:
                return std::make_shared<WaitAction>();
            case ActionType::SendChat:
                return std::make_shared<SendChatAction>();
            default:
                return nullptr;
        }
    }
}


void SpeedrunScriptingTools::DrawSettings()
{
    ToolboxPlugin::DrawSettings();
    int drawCount = 0;
    std::optional<decltype(m_scripts.begin())> scriptToDelete = std::nullopt;

    for (auto scriptIt = m_scripts.begin(); scriptIt < m_scripts.end(); ++scriptIt) {
        if (ImGui::CollapsingHeader(scriptIt->name.data())) {
            // Conditions
            std::optional<decltype(scriptIt->conditions.begin())> conditionToDelete = std::nullopt;
            for (auto it = scriptIt->conditions.begin(); it < scriptIt->conditions.end(); ++it) {
                ImGui::PushID(drawCount++);
                (*it)->drawSettings();
                ImGui::SameLine();
                if (ImGui::Button("X", ImVec2(20, 0))) {
                    conditionToDelete = it;
                }
                ImGui::PopID();
            }
            if (conditionToDelete.has_value()) scriptIt->conditions.erase(conditionToDelete.value());
            // Add condition
            if (ImGui::Button("Add condition", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                ImGui::OpenPopup("Add condition");
            }
            if (ImGui::BeginPopup("Add condition")) {
                for (auto i = 0; i < (int)ConditionType::Count; ++i) {
                    if (ImGui::Selectable(toString((ConditionType)i).data())) {
                        scriptIt->conditions.push_back(makeCondition(ConditionType(i)));
                    }
                }
                ImGui::EndPopup();
            }

            //Actions
            ImGui::Separator();
            std::optional<decltype(scriptIt->actions.begin())> actionToDelete = std::nullopt;
            for (auto it = scriptIt->actions.begin(); it < scriptIt->actions.end(); ++it) {
                ImGui::PushID(drawCount++);
                (*it)->drawSettings();
                ImGui::SameLine();
                if (ImGui::Button("X", ImVec2(20, 0))) {
                    actionToDelete = it;
                }
                ImGui::PopID();
            }
            if (actionToDelete.has_value()) scriptIt->actions.erase(actionToDelete.value());
            // Add action
            if (ImGui::Button("Add action", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                ImGui::OpenPopup("Add action");
            }
            if (ImGui::BeginPopup("Add action")) {
                for (auto i = 0; i < (int)ActionType::Count; ++i) {
                    if (ImGui::Selectable(toString((ActionType)i).data())) {
                        scriptIt->actions.push_back(makeAction(ActionType(i)));
                    }
                }
                ImGui::EndPopup();
            }
            ImGui::Separator();
            ImGui::Checkbox("Enabled", &scriptIt->enabled);
            ImGui::PushItemWidth(300);
            ImGui::InputText("Name", &scriptIt->name[0], scriptIt->name.size());
            if (ImGui::Button("Delete Script", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                scriptToDelete = scriptIt;
            }
        }
    }
    if (scriptToDelete.has_value()) m_scripts.erase(scriptToDelete.value());
}

void SpeedrunScriptingTools::LoadSettings(const wchar_t* folder)
{
    ToolboxPlugin::LoadSettings(folder);
    ini.LoadFile(GetSettingFile(folder).c_str());
    /* shortcutKey = ini.GetLongValue(Name(), "key", shortcutKey);
    shortcutMod = ini.GetLongValue(Name(), "mod", shortcutMod);
    castDelayInMs = ini.GetLongValue(Name(), "delay", castDelayInMs);*/

    //ModKeyName(hotkeyDescription, _countof(hotkeyDescription), shortcutMod, shortcutKey);
}

void SpeedrunScriptingTools::SaveSettings(const wchar_t* folder)
{
    ToolboxPlugin::SaveSettings(folder);
    /* ini.SetLongValue(Name(), "key", shortcutKey);
    ini.SetLongValue(Name(), "mod", shortcutMod);
    ini.SetLongValue(Name(), "delay", castDelayInMs);*/
    PLUGIN_ASSERT(ini.SaveFile(GetSettingFile(folder).c_str()) == SI_OK);
}

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static SpeedrunScriptingTools instance;
    return &instance;
}
void SpeedrunScriptingTools::Update(float delta)
{
    ToolboxPlugin::Update(delta);

    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        m_currentScript = std::nullopt;
        return;
    }

    if (m_currentScript && !m_currentScript->actions.empty()) {
        // Execute current script
        auto& currentActions = m_currentScript->actions;
        auto& currentAction = **currentActions.begin();
        if (currentAction.hasBeenStarted()) {
            if (currentAction.isComplete()) {
                currentAction.finalAction();
                currentActions.erase(currentActions.begin(), currentActions.begin() + 1);
            }
        }
        else {
            currentAction.initialAction();
        }
    }
    else {
        // Find script to use
        for (const auto& script : m_scripts) {
            if (!script.enabled || script.conditions.empty() || script.actions.empty()) continue;
            if (std::ranges::all_of(script.conditions, [](const auto& condition) {return condition->check();})) {
                m_currentScript = script;
                break;
            }
        }
    }
}

void SpeedrunScriptingTools::Initialize(ImGuiContext* ctx, ImGuiAllocFns fns, HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, fns, toolbox_dll);
    GW::Initialize();

    /// TODO remove
    Script testScript;
    testScript.enabled = true;
    testScript.conditions.push_back(std::make_shared<PlayerHasSkillCondition>(GW::Constants::SkillID::Ebon_Escape));
    testScript.conditions.push_back(std::make_shared<IsInMapCondition>(GW::Constants::MapID::The_Black_Curtain));
    testScript.conditions.push_back(std::make_shared<CurrentTargetIsCastingSkillCondition>(GW::Constants::SkillID::Heart_of_Shadow));       

    testScript.actions.push_back(std::make_shared<MoveToAction>(GW::GamePos{-4503.89f, 14949.46f, 0}));
    testScript.actions.push_back(std::make_shared<WaitAction>(4312));
    testScript.actions.push_back(std::make_shared<CastOnTargetAction>(GW::Constants::SkillID::Ebon_Escape));
    testScript.actions.push_back(std::make_shared<UseItemAction>(6368));
    testScript.actions.push_back(std::make_shared<CastOnSelfAction>(GW::Constants::SkillID::Shadow_Form));
    testScript.actions.push_back(std::make_shared<SendChatAction>(SendChatAction::Channel::Emote, "show settings"));

    m_scripts.push_back(std::move(testScript));
    ///
}
void SpeedrunScriptingTools::SignalTerminate()
{
    GW::DisableHooks();
}
bool SpeedrunScriptingTools::CanTerminate()
{
    return GW::HookBase::GetInHookCount() == 0;
}

void SpeedrunScriptingTools::Terminate()
{
    ToolboxPlugin::Terminate();
    GW::Terminate();
}
