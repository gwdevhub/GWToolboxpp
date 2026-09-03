#include <QuestInfo.h>

#include <Windows.h>


#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Packets/StoC.h>

#include <enumUtils.h>
#include <AsyncStringDecoder.h>

#include <ranges>
#include <chrono>

namespace {
    std::wstring getObjectiveContent(std::wstring_view enc_str)
    {
        auto content_start = enc_str.find(0x10a);
        if (content_start == std::wstring::npos) return {};
        content_start++;
        return enc_str.substr(content_start, enc_str.size() - content_start - 1).data();
    }

    GW::HookEntry ObjectiveUpdateName_Entry;
    GW::HookEntry ObjectiveDone_Entry;
    GW::HookEntry DisplayDialogue_Entry;
    GW::HookEntry QuestAdd_Entry;
    GW::HookEntry InstanceLoadFile_Entry;
    GW::HookEntry UpdateQuestnames_Entry;
} // namespace

void QuestInfo::beginDecode(std::wstring key, const std::wstring_view encoded)
{
    const auto [_, inserted] = decodedStrings.try_emplace(key, L"");
    if (!inserted) return;

    const auto generation = decodeGeneration;
    AsyncStringDecoder::Decode(encoded, [this, key = std::move(key), generation](const wchar_t* decoded) {
        if (decoded && generation == decodeGeneration) {
            decodedStrings[key] = decoded;
        }
    });
}

void QuestInfo::resetRuntime()
{
    ++decodeGeneration;
    decodedStrings.clear();
    missionObjectiveStatus.clear();
    objectivesToDecode = {};
    queuedObjectiveQuests.clear();
}

QuestStatus QuestInfo::getMissionObjectiveStatus(uint32_t id) const
{
    const auto it = missionObjectiveStatus.find(id);
    return (it != missionObjectiveStatus.end()) ? it->second : QuestStatus::NotStarted;
}

std::optional<QuestInfo::Quest> QuestInfo::getQuest(std::string_view name) const
{
    const auto questLog = GW::QuestMgr::GetQuestLog();
    if (!questLog) return {};

    for (const auto& quest : *questLog)
    {
        if (!quest.name) continue; // Fixed: passing null to decodedStrings.find implicitly constructs a wstring via wcslen, crashing if null.
        const auto decodedIt = decodedStrings.find(quest.name);
        if (decodedIt == decodedStrings.end()) continue;

        auto string = WStringToString(decodedIt->second);
        if (string.contains(name))
        {
            return QuestInfo::Quest{std::move(string), quest.quest_id};
        }
    }

    return std::nullopt;
}

std::vector<QuestInfo::Objective> QuestInfo::listObjectives(GW::Constants::QuestID id) const
{
    const auto questLog = GW::QuestMgr::GetQuestLog();
    if (!questLog) return {};

    const auto quest = std::ranges::find_if(*questLog, [&](const auto& quest) { return quest.quest_id == id; });
    if (quest == questLog->end()) return {};

    if (!quest->objectives) return {}; // Fixed: passing null to decodedStrings.find crashed for quests not yet populated.
    const auto decodedIt = decodedStrings.find(quest->objectives);
    if (decodedIt == decodedStrings.end()) return {};
    const auto& decoded = WStringToString(decodedIt->second);

    std::vector<QuestInfo::Objective> result;
    for (const auto word : std::views::split(decoded, '{'))
    {
        auto objective = std::string{word.begin(), word.end()};
        // Technically the objective now starts with "s}" or "sc}"; currently we just check "contains" on this, so dont care
        const auto isCompleted = objective.starts_with("sc");
        result.emplace_back(std::move(objective), isCompleted);
    }
    return result;
}

std::vector<QuestInfo::Objective> QuestInfo::listMissionObjectives() const
{
    const auto* worldContext = GW::GetWorldContext();
    if (!worldContext) return {}; // Fixed: was dereferenced directly; crashed during map transitions.
    const auto& objectives = worldContext->mission_objectives;
    std::vector<QuestInfo::Objective> result;
    result.reserve(objectives.size());

    for (const auto& objective : objectives)
    {
        const auto decodedIt = decodedStrings.find(objective.enc_str);
        if (decodedIt == decodedStrings.end()) continue;

        const auto status = getMissionObjectiveStatus(objective.objective_id);
        result.emplace_back(WStringToString(decodedIt->second), status == QuestStatus::Completed);
    }

    return result;
}

void QuestInfo::decodeStrings()
{
    if (const auto questLog = GW::QuestMgr::GetQuestLog())
    {
        for (auto& quest : *questLog)
        {
            // skip custom quests added by toolbox
            if (quest.map_from >= GW::Constants::MapID::Count) continue;

            if (quest.name && !decodedStrings.contains(quest.name)) {
                beginDecode(quest.name, quest.name);
            }
            if (!quest.objectives) {
                if (queuedObjectiveQuests.insert(quest.quest_id).second) {
                    objectivesToDecode.push(quest.quest_id);
                }
            }
            else if (!decodedStrings.contains(quest.objectives)) {
                beginDecode(quest.objectives, quest.objectives);
            }
        }
    }

    const auto* worldContext = GW::GetWorldContext();
    if (worldContext) { // Fixed: was dereferenced directly; called from every StoC callback so crashed on any packet during a load screen.
        for (const auto& objective : worldContext->mission_objectives)
        {
            if (!decodedStrings.contains(objective.enc_str)) {
                const auto content = getObjectiveContent(objective.enc_str);
                beginDecode(objective.enc_str, content);
            }
        }
    }
}

void QuestInfo::initialize()
{
    resetRuntime();
    decodeStrings();

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceLoadFile>(&InstanceLoadFile_Entry, [this](GW::HookStatus*, const auto*)
    {
        resetRuntime();
        decodeStrings();
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveUpdateName>(&ObjectiveUpdateName_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveUpdateName* packet) {
        this->missionObjectiveStatus[packet->objective_id] = QuestStatus::Running;
        decodeStrings();
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveDone>(&ObjectiveDone_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveDone* packet) {
        this->missionObjectiveStatus[packet->objective_id] = QuestStatus::Completed;
        decodeStrings();
    });
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::DisplayDialogue>(&DisplayDialogue_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::DisplayDialogue* packet) {
        if (wmemcmp(packet->message, L"\x8102\x5d41\xa992\xf927\x29f7", 5) == 0)
        {
             // Dhuum quest does not send a `ObjectiveUpdateName` StoC
            this->missionObjectiveStatus[157] = QuestStatus::Running;
        }
    });

    const GW::StoC::PacketCallback updateQuestNamesLambda = [this](GW::HookStatus*, const auto*) { decodeStrings(); };
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::QuestAdd>(&QuestAdd_Entry, updateQuestNamesLambda);
    GW::StoC::RegisterPostPacketCallback(&UpdateQuestnames_Entry, GAME_SMSG_QUEST_DESCRIPTION, updateQuestNamesLambda);
    GW::StoC::RegisterPostPacketCallback(&UpdateQuestnames_Entry, GAME_SMSG_QUEST_REMOVE, updateQuestNamesLambda);
    GW::StoC::RegisterPostPacketCallback(&UpdateQuestnames_Entry, GAME_SMSG_QUEST_GENERAL_INFO, updateQuestNamesLambda);
    GW::StoC::RegisterPostPacketCallback(&UpdateQuestnames_Entry, GAME_SMSG_QUEST_UPDATE_NAME, updateQuestNamesLambda);
    GW::StoC::RegisterPostPacketCallback(&UpdateQuestnames_Entry, GAME_SMSG_MISSION_OBJECTIVE_ADD, updateQuestNamesLambda);
}

void QuestInfo::terminate()
{
    // Fixed: was RemovePostCallback — wrong table for callbacks registered with RegisterPacketCallback; callbacks were never removed.
    GW::StoC::RemoveCallback<GW::Packet::StoC::ObjectiveUpdateName>(&ObjectiveUpdateName_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ObjectiveDone>(&ObjectiveDone_Entry);
    GW::StoC::RemovePostCallback<GW::Packet::StoC::InstanceLoadFile>(&InstanceLoadFile_Entry);
    GW::StoC::RemovePostCallback<GW::Packet::StoC::QuestAdd>(&QuestAdd_Entry);
    GW::StoC::RemovePostCallback<GW::Packet::StoC::DisplayDialogue>(&DisplayDialogue_Entry);
    GW::StoC::RemoveCallbacks(&UpdateQuestnames_Entry);
    resetRuntime();
}

void QuestInfo::update()
{
    if (objectivesToDecode.empty()) return;

    const auto questId = objectivesToDecode.front();
    const auto quest = GW::QuestMgr::GetQuest(questId);
    if (!quest) {
        queuedObjectiveQuests.erase(questId);
        objectivesToDecode.pop();
        return;
    }

    if (!quest->objectives) {
        GW::QuestMgr::RequestQuestInfo(quest);
        return;
    }

    beginDecode(quest->objectives, quest->objectives);
    queuedObjectiveQuests.erase(questId);
    objectivesToDecode.pop();
}
