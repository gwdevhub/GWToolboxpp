#pragma once

#include <Enums.h>

#include <commonIncludes.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Agent.h>

#include <chrono>
#include <unordered_map>

template<>
struct std::hash<std::pair<long, long>>
{
    std::size_t operator()(const std::pair<long, long>& key) const
    { 
        const auto hasher = std::hash<long>{};
        auto hash = hasher(key.first);
        // Taken from boost::hash_combine
        hash ^= hasher(key.second) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        return hash;
    }
};

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
    std::string getDecodedAgentName(GW::AgentID);
    std::string getDecodedItemName(uint32_t);

    bool canPopAgent() const;
    bool hasMinipetPopped() const;
    int getInstanceId() const { return instanceId; }
    void storeTarget(const GW::AgentLiving* agent, int storageId);
    const GW::AgentLiving* retrieveTarget(int storageId) const;

    void initialize();
    void terminate();

    void requestDisableKey(std::pair<long, long> key) { ++disabledKeys[key]; }
    void requestEnableKey(std::pair<long, long> key) { --disabledKeys[key]; }
    bool keyIsDisabled(std::pair<long, long> key) { return disabledKeys[key] != 0; }

    InstanceInfo(const InstanceInfo&) = delete;
    InstanceInfo(InstanceInfo&&) = delete;

private:
    InstanceInfo() = default;
    std::unordered_map<GW::Constants::QuestID, QuestStatus> questStatus;
    std::unordered_map<GW::AgentID, std::wstring> decodedAgentNames;
    std::unordered_map<uint32_t, std::wstring> decodedItemNames;
    std::unordered_map<int, GW::AgentID> storedTargets;
    std::unordered_map<std::pair<long, long>, int> disabledKeys;
    int instanceId = 0;
    MiniPetStatus mpStatus;
};
