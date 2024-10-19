#include "SpeedrunScriptingTools.h"

#include <ConditionIO.h>
#include <ActionIO.h>
#include <InstanceInfo.h>
#include <Keys.h>
#include <enumUtils.h>
#include <SerializationIncrement.h>
#include <ScriptVariables.h>

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
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Packets/StoC.h>

#include <imgui.h>
#include <ImGuiCppWrapper.h>
#include <SimpleIni.h>
#include <filesystem>
#include <ranges>

namespace {
    GW::HookEntry InstanceLoadFile_Entry;
    GW::HookEntry CoreMessage_Entry;
    GW::HookEntry BeginSkillCast_Entry;
    GW::HookEntry FinishSkillCast_Entry;
    GW::HookEntry Interrupt_Entry;

    struct SkillCastParameters 
    {
        uint32_t agentId;
        GW::Constants::SkillID skillId;
    };

    // Versions 1-7: Prerelease, can be ignored
    // Version 8: 1.0-1.2. See SerializationIncrement for deprecation context
    // Version 9: Skipped because exported scripts started with profanity lol
    // Version 10: 1.3-1.6. Removed when most conditions were moved to use characteristics
    // Version 11: Current.
    constexpr long currentVersion = 11;

    enum CheckType 
    {
        NonFinal,
        Final
    };
    bool checkConditions(const std::vector<ConditionPtr>& conditions, CheckType check = CheckType::NonFinal)
    {
        return std::ranges::all_of(conditions, [&](const auto& cond) {
            const auto type = cond->type();
            const auto onlyCheckInFinal = type == ConditionType::OnlyTriggerOncePerInstance || type == ConditionType::Once || type == ConditionType::Throttle;
            if ((check == CheckType::Final) == onlyCheckInFinal)
                return cond->check();
            else
                return true;
        });
    }

    std::string serialize(const Script& script) 
    {
        OutputStream stream;

        stream << 'S';
        writeStringWithSpaces(stream, script.name);
        stream << script.trigger;
        stream << script.enabled;
        switch (script.trigger) 
        {
            case Trigger::None:
            case Trigger::InstanceLoad:
            case Trigger::HardModePing:
                break;
            case Trigger::Hotkey:
                stream << script.triggerData.hotkey.keyData;
                stream << script.triggerData.hotkey.modifier;
                break;
            case Trigger::ChatMessage:
                writeStringWithSpaces(stream, script.triggerData.message);
                break;
            case Trigger::BeginCooldown:
            case Trigger::SkillCastInterrupt:
                stream << script.triggerData.skillId;
                break;
            case Trigger::BeginSkillCast:
                stream << script.triggerData.skillId;
                stream << script.triggerData.hsr;
                break;
        }

        stream << script.enabledToggleHotkey.keyData;
        stream << script.enabledToggleHotkey.modifier;
        stream << script.showMessageWhenTriggered;
        stream << script.showMessageWhenToggled;
        stream << script.globallyExclusive;
        stream << script.canLaunchInParallel;

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

    std::string serialize(const Group& group)
    {
        OutputStream stream;

        stream << 'G';
        writeStringWithSpaces(stream, group.name);
        stream << group.enabled;

        stream.writeSeparator();

        for (const auto& condition : group.conditions) {
            condition->serialize(stream);
            stream.writeSeparator();
        }

        for (const auto& script : group.scripts) 
        {
            stream << serialize(script);
            stream.writeSeparator();
        }

        return stream.str();
    }

    std::optional<Script> deserializeScript(InputStream& stream, int version = currentVersion)
    {
        Script result;

        result.name = readStringWithSpaces(stream);
        stream >> result.trigger;
        stream >> result.enabled;
        if (version == 10) 
        {
            // V10 Scripts always serialized all trigger data
            stream >> result.triggerData.hotkey.keyData;
            stream >> result.triggerData.hotkey.modifier;
            stream >> result.enabledToggleHotkey.keyData;
            stream >> result.enabledToggleHotkey.modifier;
            stream >> result.showMessageWhenTriggered;
            stream >> result.showMessageWhenToggled;
            result.triggerData.message = readStringWithSpaces(stream);
        }
        else 
        {
            switch (result.trigger) {
                case Trigger::None:
                case Trigger::InstanceLoad:
                case Trigger::HardModePing:
                    break;
                case Trigger::Hotkey:
                    stream >> result.triggerData.hotkey.keyData;
                    stream >> result.triggerData.hotkey.modifier;
                    break;
                case Trigger::ChatMessage:
                    result.triggerData.message = readStringWithSpaces(stream);
                    break;
                case Trigger::BeginCooldown:
                case Trigger::SkillCastInterrupt:
                    stream >> result.triggerData.skillId;
                    break;
                case Trigger::BeginSkillCast:
                    stream >> result.triggerData.skillId;
                    stream >> result.triggerData.hsr;
                    break;
            }
            stream >> result.enabledToggleHotkey.keyData;
            stream >> result.enabledToggleHotkey.modifier;
            stream >> result.showMessageWhenTriggered;
            stream >> result.showMessageWhenToggled;
            stream >> result.globallyExclusive;
            stream >> result.canLaunchInParallel;
        }
        stream.proceedPastSeparator();

        do {
            switch (stream.peek()) {
                case 'A':
                    stream.get();
                    if (version == 10) 
                    {
                        if (auto newAction = readV10Action(stream)) 
                            result.actions.push_back(std::move(newAction));
                    }
                    else 
                    {
                        if (auto newAction = readAction(stream)) 
                            result.actions.push_back(std::move(newAction));
                    }
                    stream.proceedPastSeparator();
                    break;
                case 'C':
                    stream.get();
                    if (version == 10) 
                    {
                        if (auto newCondition = readV10Condition(stream)) 
                            result.conditions.push_back(std::move(newCondition));
                    }
                    else 
                    {
                        if (auto newCondition = readCondition(stream)) 
                            result.conditions.push_back(std::move(newCondition));
                    }
                    stream.proceedPastSeparator();
                    break;
                default:
                    return result;
            }
        } while (stream);

        return result;
    }
    std::optional<Group> deserializeGroup(InputStream& stream)
    {
        Group result;
        std::optional<Script> nextScript;

        result.name = readStringWithSpaces(stream);
        stream >> result.enabled;
        stream.proceedPastSeparator();         

        do {
            switch (stream.peek()) {
                case 'C':
                    stream.get();
                    if (auto newCondition = readCondition(stream)) result.conditions.push_back(std::move(newCondition));
                    stream.proceedPastSeparator();
                    break;
                case 'S':
                    stream.get();
                    nextScript = deserializeScript(stream);
                    if (nextScript)
                        result.scripts.push_back(*nextScript);
                    stream.proceedPastSeparator();
                    break;
                default:
                    return result;
            }
        } while (stream);

        return result;
    }

    std::string makeScriptHeaderName(const Script& script)
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
                    result += script.triggerData.hotkey.keyData ? makeHotkeyDescription(script.triggerData.hotkey) : "Undefined hotkey";
                    break;
                case Trigger::HardModePing:
                    result += "On hard mode ping";
                    break;
                case Trigger::ChatMessage:
                    result += "On chat message \"" + script.triggerData.message + "\"";
                    break;
                case Trigger::BeginSkillCast:
                    result += "On begin casting " + getSkillName(script.triggerData.skillId, true);
                    if (script.triggerData.hsr == AnyNoYes::Yes) 
                    {
                        result += " with HSR";
                    }
                    else if (script.triggerData.hsr == AnyNoYes::No) 
                    {
                        result += " without HSR";
                    }
                    break;
                case Trigger::BeginCooldown:
                    result += "On finish casting " + getSkillName(script.triggerData.skillId, true);
                    break;
                case Trigger::SkillCastInterrupt:
                    result += "On interrupt of " + getSkillName(script.triggerData.skillId, true);
                    break;
                default:
                    result += "Unknown trigger";
            }
        }
        result += "]";
        if (script.enabledToggleHotkey.keyData)
        {
            result += "[Toggle " + makeHotkeyDescription(script.enabledToggleHotkey) + "]";
        }


        result += "###0";
        return result;
    }

    bool canBeRunInOutPost(const Script& script)
    {
        return std::ranges::all_of(script.actions, [](const auto& action)
        {
            return !action || action->behaviour().test(ActionBehaviourFlag::CanBeRunInOutpost);
        });
    }

    void drawConditionSetSelector(std::vector<ConditionPtr>& conditions) {
        using ConditionIt = decltype(conditions.begin());
        std::optional<ConditionIt> conditionToDelete = std::nullopt;
        std::optional<std::pair<ConditionIt, ConditionIt>> conditionsToSwap = std::nullopt;
        for (auto it = conditions.begin(); it < conditions.end(); ++it) {
            ImGui::PushID(it - conditions.begin() + 1);
            if (ImGui::Button("X", ImVec2(20, 0))) {
                conditionToDelete = it;
            }
                ImGui::SameLine();
                if (ImGui::Button("^", ImVec2(20, 0))) {
                    if (it != conditions.begin()) conditionsToSwap = {it - 1, it};
                }
                ImGui::SameLine();
                if (ImGui::Button("v", ImVec2(20, 0))) {
                    if (it + 1 != conditions.end()) conditionsToSwap = {it, it + 1};
                }
            ImGui::SameLine();
            (*it)->drawSettings();
            ImGui::PopID();
        }
        if (conditionToDelete.has_value()) conditions.erase(conditionToDelete.value());
        if (conditionsToSwap.has_value()) std::swap(*conditionsToSwap->first, *conditionsToSwap->second);
        // Add condition
        if (auto newCondition = drawConditionSelector(ImGui::GetContentRegionAvail().x)) {
            conditions.push_back(std::move(newCondition));
        }
    }
    void drawActionSequenceSelector(std::vector<ActionPtr>& actions) {
        using ActionIt = decltype(actions.begin());
        std::optional<ActionIt> actionToDelete = std::nullopt;
        std::optional<std::pair<ActionIt, ActionIt>> actionsToSwap = std::nullopt;
        for (auto it = actions.begin(); it < actions.end(); ++it) {
            ImGui::PushID(it - actions.begin());
            if (ImGui::Button("X", ImVec2(20, 0))) {
                actionToDelete = it;
            }
            ImGui::SameLine();
            if (ImGui::Button("^", ImVec2(20, 0))) {
                if (it != actions.begin()) actionsToSwap = {it - 1, it};
            }
            ImGui::SameLine();
            if (ImGui::Button("v", ImVec2(20, 0))) {
                if (it + 1 != actions.end()) actionsToSwap = {it, it + 1};
            }
            ImGui::SameLine();
            (*it)->drawSettings();
            ImGui::PopID();
        }
        if (actionToDelete.has_value()) actions.erase(actionToDelete.value());
        if (actionsToSwap.has_value()) std::swap(*actionsToSwap->first, *actionsToSwap->second);
        // Add action
        if (auto newAction = drawActionSelector(ImGui::GetContentRegionAvail().x)) {
            actions.push_back(std::move(newAction));
        }
    }

    struct ScriptMoveAction {
        enum class Type { Remove, Move };

        Type type;
        int scriptIndex;
        int groupIndex;
    };

    // Groups are passed for the names
    std::optional<ScriptMoveAction> drawScriptSetSelector(std::vector<Script>& scripts, const std::vector<Group>& groups, std::optional<int> groupIndex) {
        using ScriptIt = decltype(scripts.begin());
        std::optional<ScriptIt> scriptToDelete = std::nullopt;
        std::optional<std::pair<ScriptIt, ScriptIt>> scriptsToSwap = std::nullopt;

        std::optional<ScriptMoveAction> result;

        for (auto scriptIt = scripts.begin(); scriptIt < scripts.end(); ++scriptIt) {
            ImGui::PushID(scriptIt - scripts.begin());
            const auto treeHeader = makeScriptHeaderName(*scriptIt);
            const auto treeOpen = ImGui::TreeNodeEx(treeHeader.c_str(), ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_SpanAvailWidth);

            const auto offset = 127.f + (treeOpen ? 0.f : 21.f) - (groupIndex ? 20.f : 0.f);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - offset);

            if (ImGui::Button("X", ImVec2(20, 0))) {
                scriptToDelete = scriptIt;
            }
            ImGui::SameLine();
            if (ImGui::Button("^", ImVec2(20, 0)) && scriptIt != scripts.begin()) {
                scriptsToSwap = {scriptIt - 1, scriptIt};
            }
            ImGui::SameLine();
            if (ImGui::Button("v", ImVec2(20, 0)) && scriptIt + 1 != scripts.end()) {
                scriptsToSwap = {scriptIt, scriptIt + 1};
            }
            ImGui::SameLine();
            if (groupIndex) {
                if (ImGui::Button("R", ImVec2(20, 0))) result = ScriptMoveAction{ScriptMoveAction::Type::Remove, scriptIt - scripts.begin(), *groupIndex};
            }
            else {
                if (ImGui::Button("G", ImVec2(20, 0))) ImGui::OpenPopup("Group popup");
                if (ImGui::BeginPopup("Group popup")) {
                    for (auto groupIt = groups.begin(); groupIt != groups.end(); ++groupIt) {
                        if (ImGui::Selectable(groupIt->name.c_str())) result = ScriptMoveAction{ScriptMoveAction::Type::Move, scriptIt - scripts.begin(), groupIt - groups.begin()};
                    }
                    ImGui::EndPopup();
                }
            }

            if (treeOpen) {
                ImGui::PushID(0);
                drawConditionSetSelector(scriptIt->conditions);
                ImGui::PopID();
                ImGui::Separator();
                ImGui::PushID(1);
                drawActionSequenceSelector(scriptIt->actions);
                ImGui::PopID();

                ImGui::PushID(2);
                // Script settings
                ImGui::Separator();
                ImGui::Checkbox("Enabled", &scriptIt->enabled);
                ImGui::SameLine();

                auto description = scriptIt->enabledToggleHotkey.keyData ? makeHotkeyDescription(scriptIt->enabledToggleHotkey) : "Set enable toggle";
                drawHotkeySelector(scriptIt->enabledToggleHotkey, description, 80.f);
                if (scriptIt->enabledToggleHotkey.keyData) {
                    ImGui::SameLine();
                    ImGui::Text("Toggle");
                    ImGui::SameLine();
                    if (ImGui::Button("X", ImVec2(20.f, 0))) {
                        scriptIt->enabledToggleHotkey.keyData = {};
                        scriptIt->showMessageWhenToggled = false;
                    }
                    ImGui::SameLine();
                    ImGui::Checkbox("Log toggle", &scriptIt->showMessageWhenToggled);
                }
                ImGui::SameLine();

                if (ImGui::Button("Copy script", ImVec2(100, 0))) {
                    if (const auto encoded = encodeString(std::to_string(currentVersion) + " " + serialize(*scriptIt))) {
                        logMessage("Copy script " + scriptIt->name + " to clipboard");
                        ImGui::SetClipboardText(encoded->c_str());
                    }
                }

                ImGui::PopID();
                ImGui::PushID(3);

                ImGui::SameLine();
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 50);
                ImGui::InputText("Name", &scriptIt->name);
                ImGui::PopItemWidth();

                drawTriggerSelector(scriptIt->trigger, scriptIt->triggerData, 100.f);
                ImGui::SameLine();
                ImGui::Checkbox("Log trigger", &scriptIt->showMessageWhenTriggered);
                ImGui::SameLine();
                ImGui::Checkbox("Globally Exclusive", &scriptIt->globallyExclusive);
                ImGui::SameLine();
                ImGui::ShowHelp("Disallow any other script from launching while this script is running");
                ImGui::SameLine();
                ImGui::Checkbox("Can launch in parallel", &scriptIt->canLaunchInParallel);
                ImGui::PopID();

                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if (scriptToDelete.has_value()) scripts.erase(scriptToDelete.value());
        if (scriptsToSwap.has_value()) std::swap(*scriptsToSwap->first, *scriptsToSwap->second);

        return result;
    }

    std::optional<ScriptMoveAction> drawGroups(std::vector<Group>& groups)
    {
        using GroupIt = decltype(groups.begin());
        std::optional<GroupIt> groupToDelete = std::nullopt;
        std::optional<std::pair<GroupIt, GroupIt>> groupsToSwap = std::nullopt;

        std::optional<ScriptMoveAction> result;

        for (auto groupIt = groups.begin(); groupIt < groups.end(); ++groupIt) {
            ImGui::PushID(groupIt - groups.begin());
            ImGui::PushStyleColor(ImGuiCol_Header, {100.f/255, 100.f/255, 100.f/255, 0.5});

            const auto groupHeader = groupIt->name + (groupIt->enabled ? "" : " [Disabled]") + "###0";
            const auto treeOpen = ImGui::TreeNodeEx(groupHeader.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_SpanAvailWidth);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - (treeOpen ? 127.f : 148.f));
            if (ImGui::Button("X", ImVec2(20, 0))) {
                groupToDelete = groupIt;
            }
            ImGui::SameLine();
            if (ImGui::Button("^", ImVec2(20, 0)) && groupIt != groups.begin()) {
                groupsToSwap = {groupIt - 1, groupIt};
            }
            ImGui::SameLine();
            if (ImGui::Button("v", ImVec2(20, 0)) && groupIt + 1 != groups.end()) {
                groupsToSwap = {groupIt, groupIt + 1};
            }

            if (treeOpen) {
                ImGui::PushID(0);
                drawConditionSetSelector(groupIt->conditions);
                ImGui::PopID();
                if (groupIt->scripts.size() > 0) ImGui::Separator();
                ImGui::PushID(1);
                if (const auto action = drawScriptSetSelector(groupIt->scripts, groups, groupIt-groups.begin()))
                {
                    result = *action;
                }
                ImGui::PopID();

                // Group settings
                ImGui::Separator();
                ImGui::Checkbox("Enabled", &groupIt->enabled);
                ImGui::SameLine();
                if (ImGui::Button("Copy group", ImVec2(150, 0))) 
                {
                    if (const auto encoded = encodeString(std::to_string(currentVersion) + " " + serialize(*groupIt))) {
                        logMessage("Copy group " + groupIt->name + " to clipboard");
                        ImGui::SetClipboardText(encoded->c_str());
                    }
                }
                ImGui::SameLine();
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 85);
                ImGui::InputText("Group name", &groupIt->name);
                ImGui::PopItemWidth();

                ImGui::TreePop();
            }

            ImGui::PopStyleColor();
            ImGui::PopID();
        }
        if (groupToDelete.has_value()) groups.erase(groupToDelete.value());
        if (groupsToSwap.has_value()) std::swap(*groupsToSwap->first, *groupsToSwap->second);

        return result;
    }
}

void SpeedrunScriptingTools::clear() 
{
    for (auto& script : m_currentScripts)
    {
        for (auto& action : script.actions)
            action->finalAction();
    }
    m_currentScripts.clear();

    for (auto& script : m_scripts)
        script.triggered = false;
}

void SpeedrunScriptingTools::DrawSettings()
{
    ToolboxPlugin::DrawSettings();

    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading || !GW::Agents::GetControlledCharacter()) {
        return;
    }

    ImGui::PushID(0);
    const auto groupAction = drawGroups(m_groups);
    ImGui::PopID();

    if (m_groups.size() > 0) {
        ImGui::Dummy(ImVec2(0.0f, 5.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 5.0f));
    }

    ImGui::PushID(1);
    const auto scriptAction = drawScriptSetSelector(m_scripts, m_groups, std::nullopt);
    ImGui::PopID();

    if (m_scripts.size() > 0) {
        ImGui::Dummy(ImVec2(0.0f, 5.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 5.0f));
    }

    // Add script
    if (ImGui::Button("Add script", ImVec2(ImGui::GetContentRegionAvail().x / 3, 0))) {
        m_scripts.push_back({});
    }
    ImGui::SameLine();
    if (ImGui::Button("Add group", ImVec2(ImGui::GetContentRegionAvail().x / 2, 0))) {
        m_groups.push_back({});
    }
    ImGui::SameLine();
    if (ImGui::Button("Import from clipboard", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        if (const auto clipboardContent = ImGui::GetClipboardText()) {
            if (const auto combined = decodeString(clipboardContent)) {
                InputStream stream{combined.value()};

                int version = 8; // Version 8 and earlier do not write a version into the export string

                if (stream.peek() != 'S' && stream.peek() != 'G') 
                {
                    stream >> version;
                }
                while (stream && std::isspace(stream.peek()))
                {
                    stream.get();
                }
                if (stream.peek() == 'S')
                {
                    stream.get();
                    if (auto importedScript = deserializeScript(stream, version)) m_scripts.push_back(std::move(importedScript.value()));
                }
                else if (stream.peek() == 'G') 
                {
                    stream.get();
                    if (auto importedGroup = deserializeGroup(stream)) m_groups.push_back(std::move(importedGroup.value()));
                }
            }
        }
    }
    // Debug info
    auto scriptActionCounts = std::string{};
    for (const auto& script : m_currentScripts) scriptActionCounts += std::to_string(script.actions.size()) + ", ";
    ImGui::Text("Actions in queue: [%s]", scriptActionCounts.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Clear")) clear();
    ImGui::SameLine();
    ImGui::Text("Clear scripts hotkey:");
    ImGui::SameLine();
    auto description = clearScriptsKey.keyData ? makeHotkeyDescription(clearScriptsKey) : "Set key";
    drawHotkeySelector(clearScriptsKey, description, 80.f);
    if (clearScriptsKey.keyData) 
    {
        ImGui::SameLine();
        if (ImGui::Button("X", ImVec2(20.f, 0))) 
        {
            clearScriptsKey.keyData = 0;
            clearScriptsKey.modifier = 0;
        }
    }

    ImGui::Checkbox("Execute scripts while in outpost", &runInOutposts);
    ImGui::SameLine();
    ImGui::Checkbox("Block hotkey keys even if conditions not met", &alwaysBlockHotkeyKeys);

    ImGui::Text("Version 2.1.0. For new releases, feature requests and bug reports check out");
    ImGui::SameLine();

    constexpr auto discordInviteLink = "https://discord.gg/ZpKzer4dK9";
    ImGui::TextColored(ImColor{102, 187, 238, 255}, discordInviteLink);
    if (ImGui::IsItemClicked()) 
    {
        ShellExecute(nullptr, "open", discordInviteLink, nullptr, nullptr, SW_SHOWNORMAL);
    }

    const auto executeScriptMoveAction = [&](std::optional<ScriptMoveAction> action) 
    {
        if (!action) return;
        if (action->type == ScriptMoveAction::Type::Move)
        {
            m_groups[action->groupIndex].scripts.push_back(m_scripts[action->scriptIndex]);
            m_scripts.erase(m_scripts.begin() + action->scriptIndex);
        }
        else if (action->type == ScriptMoveAction::Type::Remove)
        {
            m_scripts.push_back(m_groups[action->groupIndex].scripts[action->scriptIndex]);
            m_groups[action->groupIndex].scripts.erase(m_groups[action->groupIndex].scripts.begin() + action->scriptIndex);
        }
    };

    executeScriptMoveAction(groupAction);
    executeScriptMoveAction(scriptAction);
}

void SpeedrunScriptingTools::LoadSettings(const wchar_t* folder)
{
    ToolboxPlugin::LoadSettings(folder);
    ini.LoadFile(GetSettingFile(folder).c_str());
    const long savedVersion = ini.GetLongValue(Name(), "version", 1);
    runInOutposts = ini.GetBoolValue(Name(), "runInOutpost", false);
    alwaysBlockHotkeyKeys = ini.GetBoolValue(Name(), "alwaysBlockHotkeyKeys", false);
    clearScriptsKey.keyData = ini.GetLongValue(Name(), "clearScriptsKey", 0);
    clearScriptsKey.modifier = ini.GetLongValue(Name(), "clearScriptsMod", 0);
    
    if (savedVersion == 8) logMessage("Scripts from versions before 1.3 cannot be imported");
    if (savedVersion < 10) return;
    
    if (std::string read = ini.GetValue(Name(), "scripts", ""); !read.empty()) {
        const auto decoded = decodeString(std::move(read));
        if (!decoded) return;
        InputStream stream(decoded.value());
        while (stream && stream.peek() == 'S') {
            stream.get();
            if (auto nextScript = deserializeScript(stream, savedVersion))
                m_scripts.push_back(std::move(*nextScript));
            else
                break;
        }
    }

    if (std::string read = ini.GetValue(Name(), "groups", ""); !read.empty()) {
        const auto decoded = decodeString(std::move(read));
        if (!decoded) return;
        InputStream stream(decoded.value());
        while (stream && stream.get() == 'G') {
            if (auto nextGroup = deserializeGroup(stream))
                m_groups.push_back(std::move(*nextGroup));
            else
                break;
        }
    }
}

void SpeedrunScriptingTools::SaveSettings(const wchar_t* folder)
{
    ToolboxPlugin::SaveSettings(folder);
    ini.SetLongValue(Name(), "version", currentVersion);
    ini.SetBoolValue(Name(), "runInOutpost", runInOutposts);
    ini.SetBoolValue(Name(), "alwaysBlockHotkeyKeys", alwaysBlockHotkeyKeys);
    ini.SetLongValue(Name(), "clearScriptsKey", clearScriptsKey.keyData);
    ini.SetLongValue(Name(), "clearScriptsMod", clearScriptsKey.modifier);

    {
        OutputStream stream;
        for (const auto& script : m_scripts) {
            stream << serialize(script);
        }

        if (const auto encoded = encodeString(stream.str())) {
            ini.SetValue(Name(), "scripts", encoded->c_str());   
        }
    }
    {
        OutputStream stream;
        for (const auto& group : m_groups) 
        {
            stream << serialize(group);
        }
        if (const auto encoded = encodeString(stream.str())) 
        {
            ini.SetValue(Name(), "groups", encoded->c_str());
        }
    }
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

    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading && !isInLoadingScreen) {
        // First frame on new loading screen
        isInLoadingScreen = true;
        clear();
    }

    const auto map = GW::Map::GetMapInfo();
    if (isInLoadingScreen || !map || map->GetIsPvP() || !GW::Agents::GetControlledCharacter() || (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost && !runInOutposts)) {
        return;
    }
    if (framesSinceLoadingFinished < 3) 
    {
        ++framesSinceLoadingFinished;
        return;
    }

    std::erase_if(m_currentScripts, [](const Script& s){return s.actions.empty();});
    for (auto& currentScript : m_currentScripts) 
    {
        while (currentScript.actions.size() > 0) 
        {
            // Execute current script
            auto& currentActions = currentScript.actions;
            auto& currentAction = **currentActions.begin();
            if (currentAction.behaviour().test(ActionBehaviourFlag::ImmediateFinish)) {
                currentAction.initialAction();
                currentAction.finalAction();
                currentActions.erase(currentActions.begin(), currentActions.begin() + 1);
            }
            else if (currentAction.hasBeenStarted()) {
                switch (currentAction.isComplete()) {
                    case ActionStatus::Running:
                        break;
                    case ActionStatus::CompleteAndEnteringCriticalSection:
                        currentAction.finalAction();
                        currentActions.erase(currentActions.begin(), currentActions.begin() + 1);
                        currentScript.globallyExclusive = true;
                        break;
                    case ActionStatus::CompleteAndLeavingCriticalSection:
                        currentAction.finalAction();
                        currentActions.erase(currentActions.begin(), currentActions.begin() + 1);
                        currentScript.globallyExclusive = false;
                        break;
                    case ActionStatus::Complete:
                        currentAction.finalAction();
                        currentActions.erase(currentActions.begin(), currentActions.begin() + 1);
                        break;
                    default:
                        currentAction.finalAction();
                        currentActions.clear();
                }
                break;
            }
            else {
                currentAction.initialAction();
                break;
            }
        }
    }

    const auto canRunScript = [&](const Script& script) {
        if (!script.enabled || (script.conditions.empty() && script.trigger == Trigger::None) || script.actions.empty()) return false;
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost && !canBeRunInOutPost(script)) return false;
        if (std::ranges::any_of(m_currentScripts,[&](const Script& s){ return s.getId() == script.getId();})) return false;

        return checkConditions(script.conditions);
    };
    const auto addToCurrentScripts = [&](Script& script) {
        if (script.showMessageWhenTriggered) logMessage(std::string{"Run script "} + script.name);
        script.triggered = false;
        m_currentScripts.push_back(script);
    };
    const auto hasTrigger = [](const Script& s) {
        return s.trigger != Trigger::None;
    };
    const auto isTriggered = [](const Script& s) {
        return s.triggered;
    };

    // Find script to use
    const auto checkScripts = [&](std::vector<Script>& scripts, std::vector<ConditionPtr>& groupConditions) 
    {
        if (std::ranges::any_of(m_currentScripts, [](const Script& s){ return s.globallyExclusive; })) return;
        const auto isRunningAnyScript = m_currentScripts.size() > 0;

        // Check scripts with triggers first, as these are typically more time-sensitive
        for (auto& script : scripts | std::views::filter(hasTrigger) | std::views::filter(isTriggered)) {
            if (isRunningAnyScript && !script.canLaunchInParallel) continue;
            
            script.triggered = false;

            if (!canRunScript(script))
                continue;
            if (checkConditions(script.conditions, CheckType::Final) && checkConditions(groupConditions, CheckType::Final))
                addToCurrentScripts(script);
            return;
        }
        // Run any scripts still waiting for execution
        for (auto& script : scripts | std::views::filter(std::not_fn(hasTrigger)) | std::views::filter(isTriggered)) 
        {
            if (isRunningAnyScript && !script.canLaunchInParallel) continue;
            
            script.triggered = false;

            if (!canRunScript(script)) continue;
            if (checkConditions(script.conditions, CheckType::Final) && checkConditions(groupConditions, CheckType::Final)) addToCurrentScripts(script);
            return;
        }
        
        // Trigger "Always on" scripts
        for (auto& script : scripts | std::views::filter(std::not_fn(hasTrigger))) 
        {
            script.triggered = (!isRunningAnyScript || script.canLaunchInParallel) && canRunScript(script) && checkConditions(script.conditions);
        }
    };

    for (auto& group : m_groups)
    {
        if (group.enabled && checkConditions(group.conditions)) 
            checkScripts(group.scripts, group.conditions);
    }
    std::vector<ConditionPtr> none;
    checkScripts(m_scripts, none);
}

bool SpeedrunScriptingTools::WndProc(const UINT Message, const WPARAM wParam, LPARAM lparam)
{
    if (GW::Chat::GetIsTyping() || GW::MemoryMgr::GetGWWindowHandle() != GetActiveWindow()) 
    {
        return false;
    }
    if (!runInOutposts && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) 
    {
        return false;
    }
    Hotkey pressedKey{};
    switch (Message) {
        case WM_KEYDOWN:
            if (const auto isRepeated = (int)lparam & (1 << 30)) break;
            [[fallthrough]];
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            pressedKey.keyData = static_cast<int>(wParam);
            break;
        case WM_XBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
            if (LOWORD(wParam) & MK_MBUTTON) {
                pressedKey.keyData = VK_MBUTTON;
            }
            if (LOWORD(wParam) & MK_XBUTTON1) {
                pressedKey.keyData = VK_XBUTTON1;
            }
            if (LOWORD(wParam) & MK_XBUTTON2) {
                pressedKey.keyData = VK_XBUTTON2;
            }
            break;
        case WM_XBUTTONUP:
        case WM_MBUTTONUP:
            // leave keydata to none, need to handle special case below
            break;
        case WM_MBUTTONDBLCLK:
            pressedKey.keyData = VK_MBUTTON;
             break;
        default:
            break;
    }

    if (!pressedKey.keyData) return false;

    switch (Message) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK: {
            if (GetKeyState(VK_CONTROL) < 0) {
                pressedKey.modifier |= ModKey_Control;
            }
            if (GetKeyState(VK_SHIFT) < 0) {
                pressedKey.modifier |= ModKey_Shift;
            }
            if (GetKeyState(VK_MENU) < 0) {
                pressedKey.modifier |= ModKey_Alt;
            }

            bool triggered = false;
            if (clearScriptsKey.keyData && clearScriptsKey == pressedKey) {
                clear();
                return true; // Don't set scripts to triggered if we just cleared.
            }

            const auto triggerScripts = [alwaysBlockHotkeyKeys = this->alwaysBlockHotkeyKeys, pressedKey, &triggered](std::vector<Script>& scripts) {
                for (auto& script : scripts) {
                    if (script.enabledToggleHotkey == pressedKey) {
                        if (script.showMessageWhenToggled) logMessage(script.enabled ? std::string{"Disable script "} + script.name : std::string{"Enable script "} + script.name);
                        script.enabled = !script.enabled;
                        triggered = true;
                    }

                    if (script.enabled && script.trigger == Trigger::Hotkey && script.triggerData.hotkey == pressedKey && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading) {
                        const auto conditionsMet = checkConditions(script.conditions);

                        script.triggered = conditionsMet;
                        triggered = conditionsMet || alwaysBlockHotkeyKeys;
                    }
                }
            };

            for (auto& group : m_groups) 
            {
                if (group.enabled && checkConditions(group.conditions)) 
                    triggerScripts(group.scripts);
            }
            triggerScripts(m_scripts);
        
            if (InstanceInfo::getInstance().keyIsDisabled(pressedKey)) return true;
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

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceLoadFile>(&InstanceLoadFile_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadFile*) 
    {
        ScriptVariableManager::getInstance().clear();
        isInLoadingScreen = false;
        framesSinceLoadingFinished = 0;

        const auto triggerScripts = [](auto& scripts) {
            std::ranges::for_each(scripts, [](Script& s) {
                if (s.enabled && s.trigger == Trigger::InstanceLoad) 
                    s.triggered = true;
            });
        };
        
        for (auto& group : m_groups) 
        {
            if (group.enabled)
                triggerScripts(group.scripts);
        }
        triggerScripts(m_scripts);
    });
    RegisterUIMessageCallback(&Interrupt_Entry, GW::UI::UIMessage::kSpellCastInterrupted, [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
        const auto parameters = *reinterpret_cast<SkillCastParameters*>(wparam);
        const auto player = GW::Agents::GetControlledCharacter();
        if (!player || parameters.agentId != player->agent_id) return;

        const auto triggerScripts = [&](std::vector<Script>& scripts) {
            std::ranges::for_each(scripts, [&](Script& s) {
                const auto correctSkill = s.triggerData.skillId == parameters.skillId || s.triggerData.skillId == GW::Constants::SkillID::No_Skill;
                if (correctSkill && s.enabled && s.trigger == Trigger::SkillCastInterrupt && checkConditions(s.conditions)) 
                    s.triggered = true;
            });
        };
        for (auto& group : m_groups) {
            if (group.enabled) triggerScripts(group.scripts);
        }
        triggerScripts(m_scripts);
    });
    RegisterUIMessageCallback(&FinishSkillCast_Entry, GW::UI::UIMessage::kSkillCooldownStart, [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
        struct CooldownStartMessage {
            uint32_t agentId;
            GW::Constants::SkillID skillId;
            uint32_t always0; // hero ID probably?
            float coolDownInSeconds;
        };

        const auto parameters = *reinterpret_cast<CooldownStartMessage*>(wparam);
        const auto player = GW::Agents::GetControlledCharacter();
        if (!player || parameters.agentId != player->agent_id) return;

        const auto triggerScripts = [&](std::vector<Script>& scripts) {
            std::ranges::for_each(scripts, [&](Script& s) {
                const auto correctSkill = s.triggerData.skillId == parameters.skillId || s.triggerData.skillId == GW::Constants::SkillID::No_Skill;
                if (correctSkill && s.enabled && s.trigger == Trigger::BeginCooldown && checkConditions(s.conditions)) 
                {
                    s.triggered = true;
                }
            });
        };
        for (auto& group : m_groups) {
            if (group.enabled) triggerScripts(group.scripts);
        }
        triggerScripts(m_scripts);
    });
    RegisterUIMessageCallback(&BeginSkillCast_Entry, GW::UI::UIMessage::kAgentStartCasting, [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
        struct AgentCastMessage 
        {
            uint32_t agentId;
            GW::Constants::SkillID skillId;
            float activation_time;
        };
        const auto parameters = *reinterpret_cast<AgentCastMessage*>(wparam);
        const auto player = GW::Agents::GetControlledCharacter();
        if (!player || parameters.agentId != player->agent_id) return;

        const auto triggerScripts = [&](std::vector<Script>& scripts) {
            std::ranges::for_each(scripts, [&](Script& s) {
                const auto correctSkill = s.triggerData.skillId == parameters.skillId || s.triggerData.skillId == GW::Constants::SkillID::No_Skill;
                if (!correctSkill || !s.enabled || s.trigger != Trigger::BeginSkillCast || !checkConditions(s.conditions)) return;

                if (s.triggerData.hsr == AnyNoYes::Any) {
                    s.triggered = true;
                    return;
                }

                const auto skillData = GW::SkillbarMgr::GetSkillConstantData(parameters.skillId);
                if (!skillData) return;
                const auto isHSR = skillData->activation != parameters.activation_time;
                if (isHSR == (s.triggerData.hsr == AnyNoYes::Yes)) 
                {
                    s.triggered = true;
                }
            });
        };
        for (auto& group : m_groups) {
            if (group.enabled) triggerScripts(group.scripts);
        }
        triggerScripts(m_scripts);
    });

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::MessageCore>(&CoreMessage_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::MessageCore* packet) {
        const auto triggerHardModePingScripts = [](std::vector<Script>& scripts) {
            std::ranges::for_each(scripts, [](Script& s) {
                if (s.enabled && s.trigger == Trigger::HardModePing && checkConditions(s.conditions)) s.triggered = true;
            });
        };
        const auto triggerChatMessageScripts = [&packet](std::vector<Script>& scripts) {
            std::ranges::for_each(scripts, [&](Script& s) {
                if (s.enabled && s.trigger == Trigger::ChatMessage && !s.triggerData.message.empty() && checkConditions(s.conditions)) {
                    if (WStringToString(packet->message).contains(s.triggerData.message)) {
                        s.triggered = true;
                    }
                }
            });
        };
        const auto isHardModeTrigger = wmemcmp(packet->message, L"\x8101\x7f84", 2) == 0;
        for (auto& group : m_groups) 
        {
            if (!group.enabled || !checkConditions(group.conditions)) continue;
            
            if (isHardModeTrigger) triggerHardModePingScripts(group.scripts);
            else triggerChatMessageScripts(group.scripts); 
        }
        if (isHardModeTrigger) triggerHardModePingScripts(m_scripts);
        else triggerChatMessageScripts(m_scripts);
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
    RemoveUIMessageCallback(&BeginSkillCast_Entry, GW::UI::UIMessage::kAgentStartCasting);
    RemoveUIMessageCallback(&FinishSkillCast_Entry, GW::UI::UIMessage::kSkillCooldownStart);
    RemoveUIMessageCallback(&Interrupt_Entry, GW::UI::UIMessage::kSpellCastInterrupted);
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
