#pragma once

#include <Enums.h>

#include <commonIncludes.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Agent.h>

#include <chrono>
#include <unordered_map>

template<>
struct std::hash<Hotkey>
{
    std::size_t operator()(const Hotkey& key) const
    { 
        const auto hasher = std::hash<long>{};
        auto hash = hasher(key.keyData);
        // Taken from boost::hash_combine
        hash ^= hasher(key.modifier) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
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
    QuestStatus getObjectiveStatus(uint32_t);
    std::string getDecodedAgentName(GW::AgentID);
    std::string getDecodedQuestName(GW::Constants::QuestID) const;
    std::string getDecodedItemName(uint32_t);

    bool canPopAgent() const;
    bool hasMinipetPopped() const;
    int getInstanceId() const { return instanceId; }
    void storeTarget(const GW::AgentLiving* agent, int storageId);
    const GW::AgentLiving* retrieveTarget(int storageId) const;
    bool isStoredTarget(const GW::AgentLiving& agent) const;

    void initialize();
    void terminate();

    void requestDisableKey(Hotkey key) { ++disabledKeys[key]; }
    void requestEnableKey(Hotkey key) { --disabledKeys[key]; }
    bool keyIsDisabled(Hotkey key) { return disabledKeys[key] != 0; }

    InstanceInfo(const InstanceInfo&) = delete;
    InstanceInfo(InstanceInfo&&) = delete;

private:
    InstanceInfo() = default;
    std::unordered_map<uint32_t, QuestStatus> objectiveStatus;
    std::unordered_map<GW::AgentID, std::wstring> decodedAgentNames;
    std::unordered_map<uint32_t, std::wstring> decodedItemNames;
    std::unordered_map<int, GW::AgentID> storedTargets;
    std::unordered_map<Hotkey, int> disabledKeys;
    std::unordered_map<GW::Constants::QuestID, std::wstring> questNames;
    int instanceId = 0;
    MiniPetStatus mpStatus;
};
