#include "SpeedrunScriptingTools.h"

#include <ConditionIO.h>
#include <ActionIO.h>
#include <InstanceInfo.h>
#include <GWCA/GWCA.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>

#include <imgui.h>
#include <ImGuiCppWrapper.h>
#include <SimpleIni.h>
#include <filesystem>

namespace {
    GW::HookEntry InstanceLoadFile_Entry;

    bool IsMapReady()
    {
        return GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && GW::Agents::GetPlayerAsAgentLiving();
    }
}

void SpeedrunScriptingTools::DrawSettings()
{
    ToolboxPlugin::DrawSettings();
    int drawCount = 0;
    std::optional<decltype(m_scripts.begin())> scriptToDelete = std::nullopt;

    for (auto scriptIt = m_scripts.begin(); scriptIt < m_scripts.end(); ++scriptIt) {
        const auto displayName = scriptIt->name + "###" + std::to_string(scriptIt - m_scripts.begin());
        if (ImGui::CollapsingHeader(displayName.c_str())) {
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
            ImGui::PushID(drawCount++);
            if (auto newCondition = drawConditionSelector(ImGui::GetContentRegionAvail().x)) {
                scriptIt->conditions.push_back(std::move(newCondition));
            }
            ImGui::PopID();

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
            ImGui::PushID(drawCount++);
            if (auto newAction = drawActionSelector(ImGui::GetContentRegionAvail().x)) {
                scriptIt->actions.push_back(std::move(newAction));
            }
            ImGui::PopID();

            // Add trigger packet
            drawTriggerPacketSelector(scriptIt->triggerPacket, ImGui::GetContentRegionAvail().x);
            ImGui::Separator();
            ImGui::PushID(drawCount++);
            ImGui::Checkbox("Enabled", &scriptIt->enabled);
            ImGui::PushItemWidth(300);
            ImGui::InputText("Name", &scriptIt->name);
            ImGui::SameLine();
            if (ImGui::Button("Delete Script", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                scriptToDelete = scriptIt;
            }
            ImGui::PopID();
        }
    }
    if (scriptToDelete.has_value()) m_scripts.erase(scriptToDelete.value());

    // Add script
    if (ImGui::Button("Add script", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        m_scripts.push_back({});
        m_scripts.back().name = (char)(rand() % 50 + std::min('A', 'a'));
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
        std::string token = "";
        stream >> token;
        if (token.length() > 1) return;
        switch (token[0]) {
            case 'S':
                m_scripts.push_back({});
                stream >> m_scripts.back().name;
                stream >> m_scripts.back().triggerPacket;
                stream >> m_scripts.back().enabled;
                break;
            case 'A':
                assert(m_scripts.size() > 0);
                m_scripts.back().actions.push_back(readAction(stream));
                assert(m_scripts.back().actions.back() != nullptr);
                break;
            case 'C':
                assert(m_scripts.size() > 0);
                m_scripts.back().conditions.push_back(readCondition(stream));
                assert(m_scripts.back().conditions.back() != nullptr);
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
        stream << script.name << " ";
        stream << (int)script.triggerPacket << " ";
        stream << script.enabled << " ";
        
        for (const auto& condition : script.conditions) {
            condition->serialize(stream);
        }
        for (const auto& action : script.actions) {
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

    if (!IsMapReady()) return;

    if (m_currentScript && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        m_currentScript = std::nullopt;
        return;
    }

    bool executeInstanceLoadScripts = firstFrameAfterInstanceLoad;
    firstFrameAfterInstanceLoad = false;

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
            if (script.triggerPacket != TriggerPacket::None && !(script.triggerPacket == TriggerPacket::InstanceLoad && executeInstanceLoadScripts)) continue;
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
    srand((unsigned int)time(NULL));

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceLoadFile>(&InstanceLoadFile_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadFile*) {
        firstFrameAfterInstanceLoad = true;
    });
    InstanceInfo::getInstance().initialize();
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
