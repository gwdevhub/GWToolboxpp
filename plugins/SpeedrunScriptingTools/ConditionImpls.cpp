#include <ConditionImpls.h>

#include <ConditionIO.h>
#include <utils.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/PartyMgr.h>

#include <Keys.h>

#include "windows.h"
#include "imgui.h"
#include "ImGuiCppWrapper.h"

#include <algorithm>
#include <optional>

namespace {
    constexpr double eps = 1e-3;
    const std::string missingContentToken = "/";

    std::string_view toString(QuestStatus status)
    {
        switch (status) {
            case QuestStatus::NotStarted:
                return "Not started";
            case QuestStatus::Started:
                return "Started";
            case QuestStatus::Completed:
                return "Completed";
            case QuestStatus::Failed:
                return "Failed";
        }
        return "";
    }
}

/// ------------- InvertedCondition -------------
NegatedCondition::NegatedCondition(std::istringstream& stream)
{
    std::string read;
    stream >> read;
    if (read == missingContentToken) {
        cond = nullptr;
    }
    else if (read == "C"){
        cond = readCondition(stream);
    }
    else
    {
        assert(false);
    }
}
void NegatedCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);
    if (cond)
        cond->serialize(stream);
    else
        stream << missingContentToken << " ";
}
bool NegatedCondition::check() const
{
    return cond && !cond->check();
}
void NegatedCondition::drawSettings()
{
    ImGui::Text("NOT (");
    ImGui::SameLine();
    if (cond) {
        cond->drawSettings();
    }
    else {
        cond = drawConditionSelector(100.f);
    }
    ImGui::SameLine();
    ImGui::Text(")");
}

/// ------------- DisjunctionCondition -------------
DisjunctionCondition::DisjunctionCondition(std::istringstream& stream) 
{
    std::string read;
    stream >> read;
    if (read == missingContentToken) {
        first = nullptr;
    }
    else if (read == "C") {
        first = readCondition(stream);
    }
    else {
        assert(false);
    }
    stream >> read;
    if (read == missingContentToken) {
        second = nullptr;
    }
    else if (read == "C") {
        second = readCondition(stream);
    }
    else {
        assert(false);
    }
}
void DisjunctionCondition::serialize(std::ostringstream& stream) const 
{
    Condition::serialize(stream);

    if (first)
        first->serialize(stream);
    else
        stream << missingContentToken << " ";
    
    if (second)
        second->serialize(stream);
    else
        stream << missingContentToken << " ";
}
bool DisjunctionCondition::check() const
{
    return first && second && (first->check() || second->check());
}
void DisjunctionCondition::drawSettings()
{
    ImGui::Text("(");
    ImGui::SameLine();
    if (first) {
        first->drawSettings();
    }
    else {
        ImGui::PushID(1923512);
        first = drawConditionSelector(100.f);
        ImGui::PopID();
    }
    ImGui::SameLine();
    ImGui::Text(") OR (");
    ImGui::SameLine();
    if (second) {
        second->drawSettings();
    }
    else {
        ImGui::PushID(656123);
        second = drawConditionSelector(100.f);
        ImGui::PopID();
    }
    ImGui::SameLine();
    ImGui::Text(")");
}

/// ------------- IsInMapCondition -------------
IsInMapCondition::IsInMapCondition(std::istringstream& stream)
{
    int read;
    stream >> read;
    id = (GW::Constants::MapID)read;
}
void IsInMapCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << (int)id << " ";
}
bool IsInMapCondition::check() const {
    return GW::Map::GetMapID() == id;
}
void IsInMapCondition::drawSettings() {
    ImGui::Text("If player is in map");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("id", reinterpret_cast<int*>(&id), 0);
}

/// ------------- PartyPlayerCountCondition -------------
PartyPlayerCountCondition::PartyPlayerCountCondition(std::istringstream& stream)
{
    stream >> count;
}
void PartyPlayerCountCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << count << " ";
}
bool PartyPlayerCountCondition::check() const
{
    return GW::PartyMgr::GetPartyPlayerCount() == uint32_t(count);
}
void PartyPlayerCountCondition::drawSettings()
{
    ImGui::Text("If the number of players in the party is");
    ImGui::PushItemWidth(30);
    ImGui::SameLine();
    ImGui::InputInt("", &count, 0);
}

/// ------------- InstanceProgressCondition -------------
InstanceProgressCondition::InstanceProgressCondition(std::istringstream& stream)
{
    stream >> requiredProgress;
}
void InstanceProgressCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << requiredProgress << " ";
}
bool InstanceProgressCondition::check() const {
    return GW::GetGameContext()->character->progress_bar->progress > requiredProgress;
}
void InstanceProgressCondition::drawSettings()
{
    ImGui::Text("If the instance progress (0..1) is greater than");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputFloat("", &requiredProgress, 0);
}

/// ------------- OnlyTriggerOnceCondition -------------
OnlyTriggerOnceCondition::OnlyTriggerOnceCondition(std::istringstream&)
{
}
bool OnlyTriggerOnceCondition::check() const
{
    const auto currentInstanceId = InstanceInfo::getInstance().getInstanceId();
    if (triggeredLastInInstanceId == currentInstanceId) return false;
    
    triggeredLastInInstanceId = currentInstanceId;
    return true;
}
void OnlyTriggerOnceCondition::drawSettings()
{
    ImGui::Text("If script has not been triggered in this instance");
}

/// ------------- PlayerIsNearPositionCondition -------------
PlayerIsNearPositionCondition::PlayerIsNearPositionCondition(std::istringstream& stream)
{
    stream >> pos.x >> pos.y >> accuracy;
}
void PlayerIsNearPositionCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << pos.x << " " << pos.y << " " << accuracy << " ";
}
bool PlayerIsNearPositionCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving(); 
    if (!player) return false;
    printf("distance %f", GW::GetDistance(player->pos, pos));
    return GW::GetDistance(player->pos, pos) < accuracy + eps;
}
void PlayerIsNearPositionCondition::drawSettings()
{
    ImGui::Text("If the player is near position");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputFloat("x", &pos.x, 0.0f, 0.0f);
    ImGui::SameLine();
    ImGui::InputFloat("y", &pos.y, 0.0f, 0.0f);
    ImGui::SameLine();
    ImGui::InputFloat("Accuracy", &accuracy, 0.0f, 0.0f);
}

/// ------------- PlayerHasBuffCondition -------------
PlayerHasBuffCondition::PlayerHasBuffCondition(std::istringstream& stream)
{
    int read;
    stream >> read;
    id = (GW::Constants::SkillID)read;
}
void PlayerHasBuffCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << (int)id << " ";
}
bool PlayerHasBuffCondition::check() const
{
    return GW::Effects::GetPlayerBuffBySkillId(id);
}
void PlayerHasBuffCondition::drawSettings()
{
    ImGui::Text("If the player is affected by the skill");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("id", reinterpret_cast<int*>(&id), 0);
}

/// ------------- PlayerHasSkillCondition -------------
PlayerHasSkillCondition::PlayerHasSkillCondition(std::istringstream& stream)
{
    int read;
    stream >> read;
    id = (GW::Constants::SkillID)read;
}
void PlayerHasSkillCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << (int)id << " ";
}
bool PlayerHasSkillCondition::check() const
{
    GW::Skillbar* bar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!bar || !bar->IsValid()) return false;
    for (int i = 0; i < 8; ++i) {
        if (bar->skills[i].skill_id == id) {
            return bar->skills[i].GetRecharge() == 0;
        }
    }
    return false;
}
void PlayerHasSkillCondition::drawSettings()
{
    ImGui::Text("If the player has the skill");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("id", reinterpret_cast<int*>(&id), 0);
    ImGui::SameLine();
    ImGui::Text("off cooldown");
}

/// ------------- PlayerHasClassCondition -------------
PlayerHasClassCondition::PlayerHasClassCondition(std::istringstream& stream)
{
    int read;

    stream >> read;
    primary = (Class)read;

    stream >> read;
    secondary = (Class)read;
}
void PlayerHasClassCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << (int)primary << " " << (int)secondary << " ";
}
bool PlayerHasClassCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return false;
    return (primary == Class::Any || primary == (Class)player->primary) && (secondary == Class::Any || secondary == (Class)player->secondary);
}
void PlayerHasClassCondition::drawSettings()
{
    const auto drawClassSelector = [&](Class& c, bool primary) {
        ImGui::SameLine();
        const auto id = std::string{"###"} + (primary ? "1" : "2");
        if (ImGui::Button((toString(c) + id).data(), ImVec2(100, 0))) {
            ImGui::OpenPopup((std::string{"Pick class"} + id).c_str());
        }
        if (ImGui::BeginPopup((std::string{"Pick class"} + id).c_str())) {
            for (auto i = 0; i <= (int)Class::Dervish; ++i) {
                if (ImGui::Selectable(toString((Class)i).data())) {
                    c = (Class)i;
                }
            }
            ImGui::EndPopup();
        }
    };
    ImGui::Text("If the player has primary");
    drawClassSelector(primary, true);
    ImGui::SameLine();
    ImGui::Text("and secondary");
    drawClassSelector(secondary, false);
}

/// ------------- PlayerHasNameCondition -------------
PlayerHasNameCondition::PlayerHasNameCondition(std::istringstream& stream)
{
    name = readStringWithSpaces(stream);
}
void PlayerHasNameCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    writeStringWithSpaces(stream, name);
}
bool PlayerHasNameCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return false;

    auto& instanceInfo = InstanceInfo::getInstance();
    return instanceInfo.getDecodedName(player->agent_id) == name;
}
void PlayerHasNameCondition::drawSettings()
{
    ImGui::Text("If player character has name");
    ImGui::SameLine();
    ImGui::PushItemWidth(300);
    ImGui::InputText("name", &name);
}

/// ------------- CurrentTargetIsCastingSkillCondition -------------
CurrentTargetIsCastingSkillCondition::CurrentTargetIsCastingSkillCondition(std::istringstream& stream)
{
    int read;
    stream >> read;
    id = (GW::Constants::SkillID)read;
}
void CurrentTargetIsCastingSkillCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << (int)id << " ";
}
bool CurrentTargetIsCastingSkillCondition::check() const
{
    const auto target = GW::Agents::GetTargetAsAgentLiving();
    return target && static_cast<GW::Constants::SkillID>(target->skill) == id;
}
void CurrentTargetIsCastingSkillCondition::drawSettings()
{
    ImGui::Text("If the target is casting the skill");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("id", reinterpret_cast<int*>(&id), 0);
}

/// ------------- CurrentTargetHasHpBelowCondition -------------
CurrentTargetHasHpBelowCondition::CurrentTargetHasHpBelowCondition(std::istringstream& stream)
{
    stream >> hp;
}
void CurrentTargetHasHpBelowCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << hp << " ";
}
bool CurrentTargetHasHpBelowCondition::check() const
{
    const auto target = GW::Agents::GetTargetAsAgentLiving();
    return target && target->hp < hp;
}
void CurrentTargetHasHpBelowCondition::drawSettings()
{
    ImGui::Text("If the target has HP below");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputFloat("%", &hp, 0);
}

/// ------------- HasPartyWindowAllyOfNameCondition -------------
HasPartyWindowAllyOfNameCondition::HasPartyWindowAllyOfNameCondition(std::istringstream& stream)
{
    name = readStringWithSpaces(stream);
}
void HasPartyWindowAllyOfNameCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    writeStringWithSpaces(stream, name);
}
bool HasPartyWindowAllyOfNameCondition::check() const
{
    const auto info = GW::PartyMgr::GetPartyInfo();
    const auto agentArray = GW::Agents::GetAgentArray();
    if (!info || !agentArray) return false;

    auto& instanceInfo = InstanceInfo::getInstance();
    return std::ranges::any_of(info->others, [&](const auto& allyId) {
        return instanceInfo.getDecodedName(allyId) == name;
    });
}
void HasPartyWindowAllyOfNameCondition::drawSettings()
{
    ImGui::Text("Has party window ally of name");
    ImGui::SameLine();
    ImGui::PushItemWidth(300);
    ImGui::InputText("Ally name", &name);
}

/// ------------- PartyMemberStatusCondition -------------
PartyMemberStatusCondition::PartyMemberStatusCondition(std::istringstream& stream)
{
    int read;
    stream >> read;
    status = (Status)read;
    name = readStringWithSpaces(stream);
}
void PartyMemberStatusCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << (int)status;
    writeStringWithSpaces(stream, name);
}
bool PartyMemberStatusCondition::check() const
{
    const auto info = GW::PartyMgr::GetPartyInfo();
    const auto agentArray = GW::Agents::GetAgentArray();
    if (!info || !agentArray) return false;

    auto& instanceInfo = InstanceInfo::getInstance();

    const auto agentID = [&] {
        for (const auto& player : info->players) {
            auto candidate = GW::Agents::GetAgentIdByLoginNumber(player.login_number);
            if (instanceInfo.getDecodedName(candidate) == name) {
                return std::optional{candidate};
            }
        }
        return std::optional<GW::AgentID>{};
    }();

    if (!agentID) return false;
    
    const auto agent = GW::Agents::GetAgentByID(agentID.value());
    if (!agent) return false;
    const auto living = agent->GetAsAgentLiving();

    bool shouldBeAlive = status == Status::Alive;
    return living && living->GetIsAlive() == shouldBeAlive;
}
void PartyMemberStatusCondition::drawSettings()
{
    ImGui::Text("Party window ally status");
    ImGui::SameLine();
    if (ImGui::Button(toString(status).data(), ImVec2(100, 0))) {
        ImGui::OpenPopup("Pick status");
    }
    if (ImGui::BeginPopup("Pick status")) {
        for (auto i = 0; i <= (int)Status::Alive; ++i) {
            if (ImGui::Selectable(toString((Status)i).data())) {
                status = (Status)i;
            }
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::PushItemWidth(300);
    ImGui::InputText("Ally name", &name);
}

/// ------------- QuestHasStateCondition -------------

QuestHasStateCondition::QuestHasStateCondition(std::istringstream& stream)
{
    int read;

    stream >> read;
    id = (GW::Constants::QuestID)read;

    stream >> read;
    status = (QuestStatus)read;
}
void QuestHasStateCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << (int)id << " " << (int)status << " ";
}
bool QuestHasStateCondition::check() const
{
    return InstanceInfo::getInstance().getQuestStatus(id) == status;
}
void QuestHasStateCondition::drawSettings()
{
    ImGui::Text("If the quest objective has status");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("id", reinterpret_cast<int*>(&id), 0);
    ImGui::SameLine();

    if (ImGui::Button(toString(status).data(), ImVec2(100, 0))) {
        ImGui::OpenPopup("Pick status");
    }
    if (ImGui::BeginPopup("Pick status")) {
        for (auto i = 0; i <= (int)QuestStatus::Failed; ++i) {
            if (ImGui::Selectable(toString((QuestStatus)i).data())) {
                status = (QuestStatus)i;
            }
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ShowHelp("Objective ID, NOT quest ID!\nUW: Chamber 146, Restore 147, UWG 149, Vale 150, Waste 151, Pits 152, Planes 153, Mnts 154, Pools 155");
}

/// ------------- KeyIsPressedCondition -------------
KeyIsPressedCondition::KeyIsPressedCondition(std::istringstream& stream)
{
    stream >> shortcutKey >> shortcutMod;
    ModKeyName(hotkeyDescription, _countof(hotkeyDescription), shortcutMod, shortcutKey);
}
void KeyIsPressedCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << shortcutKey << " " << shortcutMod << " ";
}
bool KeyIsPressedCondition::check() const
{
    bool keyIsPressed = GetAsyncKeyState(shortcutKey) & (1 << 15);
    if (shortcutMod & ModKey_Control) keyIsPressed &= ImGui::IsKeyDown(ImGuiKey_ModCtrl);
    if (shortcutMod & ModKey_Shift) keyIsPressed &= ImGui::IsKeyDown(ImGuiKey_ModShift);
    if (shortcutMod & ModKey_Alt) keyIsPressed &= ImGui::IsKeyDown(ImGuiKey_ModAlt);

    return keyIsPressed;
}
void KeyIsPressedCondition::drawSettings()
{
    ImGui::Text("If key is pressed:");
    ImGui::SameLine();
    if (ImGui::Button(hotkeyDescription)) {
        ImGui::OpenPopup("Select Hotkey");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to change hotkey");
    if (ImGui::BeginPopup("Select Hotkey")) {
        static long newkey = 0;
        char newkey_buf[256]{};
        long newmod = 0;

        ImGui::Text("Press key");
        if (ImGui::IsKeyDown(ImGuiKey_ModCtrl)) newmod |= ModKey_Control;
        if (ImGui::IsKeyDown(ImGuiKey_ModShift)) newmod |= ModKey_Shift;
        if (ImGui::IsKeyDown(ImGuiKey_ModAlt)) newmod |= ModKey_Alt;

        if (newkey == 0) { // we are looking for the key
            for (WORD i = 0; i < 512; i++) {
                switch (i) {
                    case VK_CONTROL:
                    case VK_LCONTROL:
                    case VK_RCONTROL:
                    case VK_SHIFT:
                    case VK_LSHIFT:
                    case VK_RSHIFT:
                    case VK_MENU:
                    case VK_LMENU:
                    case VK_RMENU:
                        continue;
                    default: {
                        if (::GetKeyState(i) & 0x8000) newkey = i;
                    }
                }
            }
        }
        else if (!(::GetKeyState(newkey) & 0x8000)) { // We have a key, check if it was released
            shortcutKey = newkey;
            shortcutMod = newmod;
            ModKeyName(hotkeyDescription, _countof(hotkeyDescription), newmod, newkey);
            newkey = 0;
            ImGui::CloseCurrentPopup();
        }

        ModKeyName(newkey_buf, _countof(newkey_buf), newmod, newkey);
        ImGui::Text("%s", newkey_buf);

        ImGui::EndPopup();
    }
}

/// ------------- InstanceTimeCondition -------------

InstanceTimeCondition::InstanceTimeCondition(std::istringstream& stream)
{
    stream >> timeInSeconds;
}
void InstanceTimeCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << timeInSeconds << " ";
}
bool InstanceTimeCondition::check() const
{
    return (int)(GW::Map::GetInstanceTime() / 1000) >= timeInSeconds;
}
void InstanceTimeCondition::drawSettings()
{
    ImGui::Text("If the instance is older than");
    ImGui::SameLine();
    ImGui::InputInt("s", &timeInSeconds, 0);
}
