#pragma once

#include <Enums.h>

#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <optional>

namespace GW {
    namespace Constants {
        enum class QuestID : uint32_t;
    }
    struct Quest;
}

// Abstracts away the logic of finding and getting the status/name of quests in the quest log
class QuestInfo {
public:
    static QuestInfo& getInstance()
    {
        static QuestInfo info;
        return info;
    }

    struct Quest 
    {
        std::string name;
        GW::Constants::QuestID id;
    };

    struct Objective 
    {
        std::string name;
        bool isCompleted = false;
    };

    QuestStatus getMissionObjectiveStatus(uint32_t) const;
    std::optional<Quest> getQuest(std::string_view) const;
    std::vector<Objective> listObjectives(GW::Constants::QuestID) const;
    std::vector<Objective> listMissionObjectives() const;
    

    void initialize();
    void terminate();
    void update();

    QuestInfo(const QuestInfo&) = delete;
    QuestInfo(QuestInfo&&) = delete;

private:
    void beginDecode(std::wstring key, std::wstring_view encoded);
    void decodeStrings();
    void resetRuntime();

    QuestInfo() = default;

    std::unordered_map<std::wstring, std::wstring> decodedStrings;
    std::unordered_map<uint32_t, QuestStatus> missionObjectiveStatus;

    std::queue<GW::Constants::QuestID> objectivesToDecode;
    std::unordered_set<GW::Constants::QuestID> queuedObjectiveQuests;
    uint64_t decodeGeneration = 0;
};
