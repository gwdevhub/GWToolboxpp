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
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>

#include <imgui.h>
#include <ImGuiCppWrapper.h>
#include <SimpleIni.h>
#include <filesystem>

namespace {
    GW::HookEntry InstanceLoadFile_Entry;
    GW::HookEntry CoreMessage_Entry;

    constexpr long currentVersion = 8;

    bool canAddCondition(const Script& script) {
        return !std::ranges::any_of(script.conditions, [](const auto& cond) {
            return cond->type() == ConditionType::OnlyTriggerOncePerInstance;
        });
    }

    bool checkConditions(const Script& script) {
        return std::ranges::all_of(script.conditions, [](const auto& cond) {
            return cond->check();
        });
    }

    std::string serialize(const Script& script) 
    {
        OutputStream stream;

        stream << 'S';
        writeStringWithSpaces(stream, script.name);
        stream << script.trigger;
        stream << script.enabled;
        stream << script.hotkeyStatus.keyData;
        stream << script.hotkeyStatus.modifier;

        stream.writeSeparator();

        for (const auto& condition : script.conditions) {
            condition->serialize(stream);
            stream.writeSeparator();
        }
        for (const auto& action : script.actions) {
            action->serialize(stream);
            stream.writeSeparator();
        }
        return stream.str();
    }

    std::optional<Script> deserialize(InputStream& stream)
    {
        std::optional<Script> result;

        do {
            if (result.has_value() && stream.peek() == 'S') return result;
            std::string token = "";
            stream >> token;
            if (token.length() != 1) return result;
            switch (token[0]) {
                case 'S':
                    assert(!result.has_value());
                    result = Script{};
                    result->name = readStringWithSpaces(stream);
                    stream >> result->trigger;
                    stream >> result->enabled;
                    stream >> result->hotkeyStatus.keyData;
                    stream >> result->hotkeyStatus.modifier;
                    stream.proceedPastSeparator();
                    break;
                case 'A':
                    if (!result) return std::nullopt;
                    if (auto newAction = readAction(stream))
                        result->actions.push_back(std::move(newAction));
                    stream.proceedPastSeparator();
                    break;
                case 'C':
                    if (!result) return std::nullopt;
                    if (auto newCondition = readCondition(stream))
                        result->conditions.push_back(std::move(newCondition));
                    stream.proceedPastSeparator();
                    break;
                default:
                    return result;
            }
        } while (stream);

        return result;
    }

    std::string makeScriptHeaderName(const Script& script, int id)
    {
        std::string result = script.name + " [";
        if (!script.enabled) 
        {
            result += "Disabled";
        }
        else {
            switch (script.trigger) {
                case Trigger::None:
                    result += "Always on";
                    break;
                case Trigger::InstanceLoad:
                    result += "On instance load";
                    break;
                case Trigger::Hotkey:
                    if (script.hotkeyStatus.keyData) 
                    {
                        result += makeHotkeyDescription(script.hotkeyStatus.keyData, script.hotkeyStatus.modifier);
                    }
                    else 
                    {
                        result += "Undefined hotkey";
                    }
                    break;
                case Trigger::HardModePing:
                    result += "On hard mode ping";
                    break;
                default:
                    result += "Unknown trigger";
            }
        }

        result += "]###" + std::to_string(id);
        return result;
    }

    void logMessage(std::string_view message)
    {
        const auto wMessage = std::wstring{message.begin(), message.end()};
        const size_t len = 30 + wcslen(wMessage.c_str());
        auto to_send = new wchar_t[len];
        swprintf(to_send, len - 1, L"<a=1>%s</a><c=#%6X>: %s</c>", L"SST", 0xFFFFFF, wMessage.c_str());   
        GW::GameThread::Enqueue([to_send]{
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GWCA2, to_send, nullptr);
            delete[] to_send;
        });
    }
}

void SpeedrunScriptingTools::DrawSettings()
{
    ToolboxPlugin::DrawSettings();
    std::optional<decltype(m_scripts.begin())> scriptToDelete = std::nullopt;

    for (auto scriptIt = m_scripts.begin(); scriptIt < m_scripts.end(); ++scriptIt) {
        if (ImGui::TreeNodeEx(makeScriptHeaderName(*scriptIt, scriptIt - m_scripts.begin()).c_str(), ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            // Conditions
            {
                using ConditionIt = decltype(scriptIt->conditions.begin());
                std::optional<ConditionIt> conditionToDelete = std::nullopt;
                std::optional<std::pair<ConditionIt, ConditionIt>> conditionsToSwap = std::nullopt;
                for (auto it = scriptIt->conditions.begin(); it < scriptIt->conditions.end(); ++it) {
                    ImGui::PushID(it - scriptIt->conditions.begin());
                    if (ImGui::Button("X", ImVec2(20, 0))) {
                        conditionToDelete = it;
                    }
                    if (it->get()->type() != ConditionType::OnlyTriggerOncePerInstance) {
                        ImGui::SameLine();
                        if (ImGui::Button("^", ImVec2(20, 0))) {
                            if (it != scriptIt->conditions.begin()) conditionsToSwap = {it - 1, it};
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("v", ImVec2(20, 0))) {
                            if (it + 1 != scriptIt->conditions.end() && (it+1)->get()->type() != ConditionType::OnlyTriggerOncePerInstance) conditionsToSwap = {it, it + 1};
                        }
                    }
                    ImGui::SameLine();
                    (*it)->drawSettings();
                    ImGui::PopID();
                }
                if (conditionToDelete.has_value()) scriptIt->conditions.erase(conditionToDelete.value());
                if (conditionsToSwap.has_value()) std::swap(*conditionsToSwap->first, *conditionsToSwap->second);
                // Add condition
                if (canAddCondition(*scriptIt)) {
                    if (auto newCondition = drawConditionSelector(ImGui::GetContentRegionAvail().x)) {
                        scriptIt->conditions.push_back(std::move(newCondition));
                    }
                }
            }

            //Actions
            ImGui::Separator();
            {
                using ActionIt = decltype(scriptIt->actions.begin());
                std::optional<ActionIt> actionToDelete = std::nullopt;
                std::optional<std::pair<ActionIt, ActionIt>> actionsToSwap = std::nullopt;
                for (auto it = scriptIt->actions.begin(); it < scriptIt->actions.end(); ++it) {
                    ImGui::PushID(it - scriptIt->actions.begin() + scriptIt->conditions.size());
                    if (ImGui::Button("X", ImVec2(20, 0))) {
                        actionToDelete = it;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("^", ImVec2(20, 0))) {
                        if (it != scriptIt->actions.begin()) actionsToSwap = {it - 1, it};
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("v", ImVec2(20, 0))) {
                        if (it + 1 != scriptIt->actions.end()) actionsToSwap = {it, it + 1};
                    }
                    ImGui::SameLine();
                    (*it)->drawSettings();
                    ImGui::PopID();
                }
                if (actionToDelete.has_value()) scriptIt->actions.erase(actionToDelete.value());
                if (actionsToSwap.has_value()) std::swap(*actionsToSwap->first, *actionsToSwap->second);
                // Add action
                if (auto newAction = drawActionSelector(ImGui::GetContentRegionAvail().x)) {
                    scriptIt->actions.push_back(std::move(newAction));
                }
            }

            // Add trigger packet
            ImGui::Separator();
            {
                ImGui::Checkbox("Enabled", &scriptIt->enabled);
                ImGui::PushItemWidth(150);
                ImGui::SameLine();
                ImGui::InputText("Name", &scriptIt->name);
                ImGui::SameLine();
                drawTriggerSelector(scriptIt->trigger, ImGui::GetContentRegionAvail().x / 3, scriptIt->hotkeyStatus.keyData, scriptIt->hotkeyStatus.modifier);
                ImGui::SameLine();
                if (ImGui::Button("Copy script to clipboard", ImVec2(ImGui::GetContentRegionAvail().x / 2, 0))) {
                    ImGui::SetClipboardText(encodeString(serialize(*scriptIt)).c_str());
                }
                ImGui::SameLine();
                if (ImGui::Button("Delete Script", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                    scriptToDelete = scriptIt;
                }
            }

            ImGui::TreePop();
        }
    }
    if (scriptToDelete.has_value()) m_scripts.erase(scriptToDelete.value());

    ImGui::Dummy(ImVec2(0.0f, 5.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 5.0f));

    // Add script
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x / 2);
    if (ImGui::Button("Add script", ImVec2(ImGui::GetContentRegionAvail().x / 2, 0))) {
        m_scripts.push_back({});
    }
    ImGui::SameLine();
    if (ImGui::Button("Import script from clipboard", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        if (const auto clipboardContent = ImGui::GetClipboardText()) {
            const auto combined = decodeString(clipboardContent);
            InputStream stream{combined};
            if (auto importedScript = deserialize(stream))
                m_scripts.push_back(std::move(importedScript.value()));
        }
    }
    // Debug info
    ImGui::Text("Current action: %s", (m_currentScript.has_value() && !m_currentScript->actions.empty()) ? toString(m_currentScript->actions.front()->type()).data() : "None");
    ImGui::SameLine();
    if (ImGui::Button("Clear")) 
    {
        m_currentScript = std::nullopt;
    }
    ImGui::SameLine();
    ImGui::Text("Actions in queue: %i", m_currentScript ? m_currentScript->actions.size() : 0u);
    ImGui::SameLine();
    ImGui::Checkbox("Show message when a script is triggered", &showTriggerMessage);

    ImGui::Text("Version 1.1-bay6TargetSameFix. For new releases, feature requests and bug reports check out");
    ImGui::SameLine();

    constexpr auto discordInviteLink = "https://discord.gg/ZpKzer4dK9";
    ImGui::TextColored(ImColor{102, 187, 238, 255}, discordInviteLink);
    if (ImGui::IsItemClicked()) 
    {
        ShellExecute(nullptr, "open", discordInviteLink, nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void SpeedrunScriptingTools::LoadSettings(const wchar_t* folder)
{
    ToolboxPlugin::LoadSettings(folder);
    ini.LoadFile(GetSettingFile(folder).c_str());
    const long savedVersion = ini.GetLongValue(Name(), "version", 1);
    showTriggerMessage = ini.GetBoolValue(Name(), "showTriggerMessage", showTriggerMessage);
    
    if (savedVersion != currentVersion) return;
    
    std::string read = ini.GetValue(Name(), "scripts", "");
    if (read.empty()) return;

    const auto decoded = decodeString(std::move(read));

    InputStream stream(decoded);
    while (stream) {
        auto nextScript = deserialize(stream);
        if (nextScript) {
            m_scripts.push_back(std::move(*nextScript));
        }
        else {
            break;
        }
    }
}

void SpeedrunScriptingTools::SaveSettings(const wchar_t* folder)
{
    ToolboxPlugin::SaveSettings(folder);
    ini.SetLongValue(Name(), "version", currentVersion);
    ini.SetBoolValue(Name(), "showTriggerMessage", showTriggerMessage);

    OutputStream stream;
    for (const auto& script : m_scripts) 
    {
        stream << serialize(script);
    }
    
    ini.SetValue(Name(), "scripts", encodeString(stream.str()).c_str());
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

    if (m_currentScript && !m_currentScript->actions.empty()) {
        // Execute current script
        auto& currentActions = m_currentScript->actions;
        auto& currentAction = **currentActions.begin();
        if (currentAction.hasBeenStarted()) {
            switch (currentAction.isComplete()) 
            {
                case ActionStatus::Running:
                    break;
                case ActionStatus::Complete:
                    currentAction.finalAction();
                    currentActions.erase(currentActions.begin(), currentActions.begin() + 1);
                    break;
                default:
                    currentAction.finalAction();
                    currentActions.clear();
            }
        }
        else {
            currentAction.initialAction();
        }
    }
    else {
        // Find script to use
        for (auto& script : m_scripts) {
            if (!script.enabled || (script.conditions.empty() && script.trigger == Trigger::None) || script.actions.empty()) continue;
            if (script.trigger == currentTrigger || (script.trigger == Trigger::Hotkey && script.hotkeyStatus.triggered)) {
                script.hotkeyStatus.triggered = false;
                if (checkConditions(script)) {
                    if (showTriggerMessage) 
                        logMessage(std::string{"Trigger script "} + script.name);
                    m_currentScript = script;
                    break;
                }
            }
        }
    }

    currentTrigger = Trigger::None;
    for (auto& script : m_scripts)
        script.hotkeyStatus.triggered = false;
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
        if (std::ranges::any_of(m_scripts, [](const Script& s){return s.enabled && s.trigger == Trigger::InstanceLoad;}))
            currentTrigger = Trigger::InstanceLoad;
    });
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::MessageCore>(&CoreMessage_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::MessageCore* packet) {
        if (wmemcmp(packet->message, L"\x8101\x7f84", 2) == 0) {
            if (std::ranges::any_of(m_scripts, [](const Script& s){return s.enabled && s.trigger == Trigger::HardModePing;}))
                currentTrigger = Trigger::HardModePing;
        }
    });
    InstanceInfo::getInstance().initialize();
    srand((unsigned int)time(NULL));
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
