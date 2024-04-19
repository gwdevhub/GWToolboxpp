#include "SpeedrunScriptingTools.h"

#include <ConditionIO.h>
#include <ActionIO.h>
#include <InstanceInfo.h>
#include <utils.h>
#include <Keys.h>

#include <GWCA/GWCA.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Hook.h>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>

#include <imgui.h>
#include <ImGuiCppWrapper.h>
#include <SimpleIni.h>
#include <filesystem>

namespace {
    GW::HookEntry InstanceLoadFile_Entry;
    GW::HookEntry CoreMessage_Entry;

    constexpr long currentVersion = 2;

    bool canAddCondition(const Script& script) {
        return !std::ranges::any_of(script.conditions, [](const auto& cond) {
            return cond->type() == ConditionType::OnlyTriggerOncePerInstance;
        });
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
            if (canAddCondition(*scriptIt)) {
                ImGui::PushID(drawCount++);
                if (auto newCondition = drawConditionSelector(ImGui::GetContentRegionAvail().x)) {
                    scriptIt->conditions.push_back(std::move(newCondition));
                }
                ImGui::PopID();
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
            ImGui::PushID(drawCount++);
            if (auto newAction = drawActionSelector(ImGui::GetContentRegionAvail().x)) {
                scriptIt->actions.push_back(std::move(newAction));
            }
            ImGui::PopID();

            // Add trigger packet
            ImGui::Separator();
            ImGui::PushID(drawCount++);
            ImGui::Checkbox("Enabled", &scriptIt->enabled);
            ImGui::PushItemWidth(200);
            ImGui::SameLine();
            ImGui::InputText("Name", &scriptIt->name);
            ImGui::SameLine();
            drawTriggerSelector(scriptIt->trigger, ImGui::GetContentRegionAvail().x / 2, scriptIt->hotkeyStatus.keyData, scriptIt->hotkeyStatus.modifier);
            ImGui::SameLine();
            if (ImGui::Button("Delete Script", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                scriptToDelete = scriptIt;
            }
            ImGui::PopID();
        }
    }
    if (scriptToDelete.has_value()) m_scripts.erase(scriptToDelete.value());

    ImGui::Dummy(ImVec2(0.0f, 5.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 5.0f));

    // Add script
    if (ImGui::Button("Add script", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        m_scripts.push_back({});
        m_scripts.back().name = "New script";
    }

    ImGui::Text("Version 0.2. For bug reports and requests contact Jabor.");
}

void SpeedrunScriptingTools::LoadSettings(const wchar_t* folder)
{
    ToolboxPlugin::LoadSettings(folder);
    ini.LoadFile(GetSettingFile(folder).c_str());
    const long savedVersion = ini.GetLongValue(Name(), "version", 1);
    
    if (savedVersion != currentVersion) return;
    
    const std::string read = ini.GetValue(Name(), "scripts", "");
    if (read.empty()) return;

    std::istringstream stream(read);
    do { 
        std::string token = "";
        stream >> token;
        if (token.length() != 1) return;
        switch (token[0]) {
            case 'S':
                m_scripts.push_back({});
                m_scripts.back().name = readStringWithSpaces(stream);
                stream >> m_scripts.back().trigger;
                stream >> m_scripts.back().enabled;
                stream >> m_scripts.back().hotkeyStatus.keyData;
                stream >> m_scripts.back().hotkeyStatus.modifier;
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
    ini.SetLongValue(Name(), "version", currentVersion);

    std::ostringstream stream;
    for (const auto& script : m_scripts) {
        stream << 'S' << " ";
        writeStringWithSpaces(stream, script.name);
        stream << (int)script.trigger << " ";
        stream << script.enabled << " ";
        stream << script.hotkeyStatus.keyData << " ";
        stream << script.hotkeyStatus.modifier << " ";
        
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

    const auto map = GW::Map::GetMapInfo();
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable || !map || map->GetIsPvP() || !GW::Agents::GetPlayerAsAgentLiving()) {
        m_currentScript = std::nullopt;
        for (auto& script : m_scripts)
            script.hotkeyStatus.triggered = false;
        return;
    }

    auto currentTrigger = Trigger::None;
    if (firstFrameAfterInstanceLoad) {
        currentTrigger = Trigger::InstanceLoad;
        firstFrameAfterInstanceLoad = false;
    }
    else if (firstFrameAfterHardModePing) {
        currentTrigger = Trigger::HardModePing;
        firstFrameAfterHardModePing = false;
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
        for (auto& script : m_scripts) {
            if (!script.enabled || script.conditions.empty() || script.actions.empty()) continue;
            if (script.trigger == currentTrigger || (script.trigger == Trigger::Hotkey && script.hotkeyStatus.triggered)) {
                script.hotkeyStatus.triggered = false;
                if (std::ranges::all_of(script.conditions, [](const auto& condition) {return condition->check();})) {
                    m_currentScript = script;
                    break;
                }
            }
        }
    }
}

bool SpeedrunScriptingTools::WndProc(const UINT Message, const WPARAM wParam, LPARAM lparam)
{
    if (GW::Chat::GetIsTyping()) {
        return false;
    }
    if (GW::MemoryMgr::GetGWWindowHandle() != GetActiveWindow()) {
        return false;
    }
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
        return false;
    }
    long keyData = 0;
    switch (Message) {
        case WM_KEYDOWN:
            if (const auto isRepeated = (int)lparam & (1 << 30)) break;
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            keyData = static_cast<int>(wParam);
            break;
        case WM_XBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
            if (LOWORD(wParam) & MK_MBUTTON) {
                keyData = VK_MBUTTON;
            }
            if (LOWORD(wParam) & MK_XBUTTON1) {
                keyData = VK_XBUTTON1;
            }
            if (LOWORD(wParam) & MK_XBUTTON2) {
                keyData = VK_XBUTTON2;
            }
            break;
        case WM_XBUTTONUP:
        case WM_MBUTTONUP:
            // leave keydata to none, need to handle special case below
            break;
        case WM_MBUTTONDBLCLK:
             keyData = VK_MBUTTON;
             break;
        default:
            break;
    }

    switch (Message) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK: {
            long modifier = 0;
            if (GetKeyState(VK_CONTROL) < 0) {
                modifier |= ModKey_Control;
            }
            if (GetKeyState(VK_SHIFT) < 0) {
                modifier |= ModKey_Shift;
            }
            if (GetKeyState(VK_MENU) < 0) {
                modifier |= ModKey_Alt;
            }

            bool triggered = false;
            for (auto& script : m_scripts) {
                if (script.enabled && script.trigger == Trigger::Hotkey && script.hotkeyStatus.keyData == keyData && 
                    script.hotkeyStatus.modifier == modifier) {
                    script.hotkeyStatus.triggered = true;
                    triggered = true;
                }
            }
            return triggered;
        }

        case WM_KEYUP:
        case WM_SYSKEYUP:

        case WM_XBUTTONUP:
        case WM_MBUTTONUP:
        default:
            return false;
    }
}

void SpeedrunScriptingTools::Initialize(ImGuiContext* ctx, ImGuiAllocFns fns, HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, fns, toolbox_dll);
    GW::Initialize();

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceLoadFile>(
        &InstanceLoadFile_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadFile*) {
        firstFrameAfterInstanceLoad = true;
    });
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::MessageCore>(&CoreMessage_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::MessageCore* packet) {
        if (wmemcmp(packet->message, L"\x8101\x7f84", 2) == 0) {
            firstFrameAfterHardModePing = true;
        }
    });
    InstanceInfo::getInstance().initialize();
}
void SpeedrunScriptingTools::SignalTerminate()
{
    ToolboxPlugin::SignalTerminate();

    InstanceInfo::getInstance().terminate();
    GW::StoC::RemovePostCallback<GW::Packet::StoC::InstanceLoadFile>(&InstanceLoadFile_Entry);
    GW::StoC::RemovePostCallback<GW::Packet::StoC::MessageCore>(&CoreMessage_Entry);
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
