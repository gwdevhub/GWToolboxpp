#pragma once

#include <utils.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Agent.h>

#include <chrono>
#include <unordered_map>

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
    void update();

    bool canPopAgent() const;
    bool hasMinipetPopped() const;
    int getInstanceId() const { return instanceId; }
    const std::unordered_map<GW::AgentID, std::string>& getPlayerNames() const { return playerDecodedNames; }
    void storeTarget(const GW::AgentLiving* agent, int storageId);
    const GW::AgentLiving* retrieveTarget(int storageId) const;

    void initialize();
    void terminate();

    InstanceInfo(const InstanceInfo&) = delete;
    InstanceInfo(InstanceInfo&&) = delete;

private:
    InstanceInfo() = default;
    std::unordered_map<GW::Constants::QuestID, QuestStatus> questStatus;
    std::unordered_map<GW::AgentID, std::wstring> decodedNames;
    std::unordered_map<GW::AgentID, std::string> playerDecodedNames;
    std::unordered_map<int, GW::AgentID> storedTargets;
    int instanceId = 0;
    MiniPetStatus mpStatus;
};
