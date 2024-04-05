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
                return std::make_shared<QuestHasStateCondition>();
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

     std::shared_ptr<Action> readAction(std::istringstream& stream) 
    {
        int type;
        
        stream >> type;
        switch (static_cast<ActionType>(type)) {
            case ActionType::MoveTo:
                return std::make_shared<MoveToAction>(stream);
            case ActionType::CastOnSelf:
                return std::make_shared<CastOnSelfAction>(stream);
            case ActionType::CastOnTarget:
                return std::make_shared<CastOnTargetAction>(stream);
            case ActionType::UseItem:
                return std::make_shared<UseItemAction>(stream);
            case ActionType::SendDialog:
                return std::make_shared<SendDialogAction>(stream);
            case ActionType::GoToNpc:
                return std::make_shared<GoToNpcAction>(stream);
            case ActionType::Wait:
                return std::make_shared<WaitAction>(stream);
            case ActionType::SendChat:
                return std::make_shared<SendChatAction>(stream);
            default:
                return nullptr;
        }
    }

    std::shared_ptr<Condition> readCondition(std::istringstream& stream)
    {
        int type;
        stream >> type;
        switch (static_cast<ConditionType>(type)) {
            case ConditionType::IsInMap:
                return std::make_shared<IsInMapCondition>(stream);
            case ConditionType::QuestHasState:
                return std::make_shared<QuestHasStateCondition>(stream);
            case ConditionType::PartyPlayerCount:
                return std::make_shared<PartyPlayerCountCondition>(stream);
            case ConditionType::InstanceProgress:
                return std::make_shared<InstanceProgressCondition>(stream);
            case ConditionType::HasPartyWindowAllyOfName:
                return nullptr;

            case ConditionType::PlayerIsNearPosition:
                return std::make_shared<PlayerIsNearPositionCondition>(stream);
            case ConditionType::PlayerHasBuff:
                return std::make_shared<PlayerHasBuffCondition>(stream);
            case ConditionType::PlayerHasSkill:
                return std::make_shared<PlayerHasSkillCondition>(stream);

            case ConditionType::CurrentTargetHasHpPercentBelow:
                return nullptr;
            case ConditionType::CurrentTargetIsUsingSkill:
                return std::make_shared<CurrentTargetIsCastingSkillCondition>(stream);

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
            ImGui::SameLine();
            if (ImGui::Button("Delete Script", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                scriptToDelete = scriptIt;
            }
        }
    }
    if (scriptToDelete.has_value()) m_scripts.erase(scriptToDelete.value());

    // Add script
    if (ImGui::Button("Add script", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        ImGui::OpenPopup("Add script");
    }
    if (ImGui::BeginPopup("Add script")) {
        if (m_scripts.size() < 3) {
            m_scripts.push_back({});
            m_scripts.back().name = (char)(rand() % 50 + std::min('A', 'a'));
        }
        ImGui::EndPopup();
    }
}

void SpeedrunScriptingTools::LoadSettings(const wchar_t* folder)
{
    ToolboxPlugin::LoadSettings(folder);
    ini.LoadFile(GetSettingFile(folder).c_str());
    const std::string read = ini.GetValue(Name(), "scripts", "");
    if (read.empty()) return;

    std::istringstream stream(read);
    do { 
        char token;
        stream >> token;
        switch (token) {
            case 'S':
                m_scripts.push_back({});
                stream >> m_scripts.back().enabled;
                stream >> m_scripts.back().name;
                break;
            case 'A':
                if (m_scripts.empty()) return;
                m_scripts.back().actions.push_back(readAction(stream));
                break;
            case 'C':
                assert(m_scripts.size() > 0);
                m_scripts.back().conditions.push_back(readCondition(stream));
                break;
            default:
                return;
        }
    } while (stream); 
}

void SpeedrunScriptingTools::SaveSettings(const wchar_t* folder)
{
    ToolboxPlugin::SaveSettings(folder);
    std::ostringstream stream;
    for (const auto& script : m_scripts) {
        stream << 'S' << " ";
        stream << script.enabled << " ";
        stream << script.name << " ";
        for (const auto& condition : script.conditions) {
            stream << 'C' << " ";
            stream << (int)condition->type() << " ";
            condition->serialize(stream);
        }
        for (const auto& action : script.actions) {
            stream << 'A' << " ";
            stream << (int)action->type() << " ";
            action->serialize(stream);
        }
    }
    
    ini.SetValue(Name(), "scripts", stream.str().c_str());
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
