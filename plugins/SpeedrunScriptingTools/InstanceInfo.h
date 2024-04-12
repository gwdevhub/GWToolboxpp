#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Agent.h>

#include <unordered_map>
#include <chrono>

enum class QuestStatus : int { NotStarted, Started, Completed, Failed };

// Contains information about the current instance which either has to be kept between function calls or is expensive to compute
class InstanceInfo {
public:
    struct MiniPetStatus {
        std::optional<uint32_t> poppedMinipetId = std::nullopt;
        std::chrono::time_point<std::chrono::steady_clock> lastPop = std::chrono::steady_clock::now();
    };

    static InstanceInfo& getInstance() { 
        static InstanceInfo info;
        return info;
    }
    QuestStatus getQuestStatus(GW::Constants::QuestID);
    std::string getDecodedName(GW::AgentID);
    bool canPopAgent() const;
    bool hasMinipetPopped() const;
    int getInstanceId() { return instanceId; }
    void initialize();

    InstanceInfo(const InstanceInfo&) = delete;
    InstanceInfo(InstanceInfo&&) = delete;

private:
    InstanceInfo() = default;
    std::unordered_map<GW::Constants::QuestID, QuestStatus> questStatus;
    std::unordered_map<GW::AgentID, std::wstring> decodedNames;
    int instanceId = 0;
    MiniPetStatus mpStatus;
};
