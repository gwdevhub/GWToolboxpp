#pragma once

#include <sstream>

enum class ConditionType : int {
    // Logic Operators
    Not,
    Or,

    // Instance state
    IsInMap,
    QuestHasState,
    PartyPlayerCount,
    PartyMemberStatus,
    HasPartyWindowAllyOfName,
    InstanceProgress,
    InstanceTime,
    OnlyTriggerOncePerInstance,
    CanPopAgent,

    // Player state
    PlayerIsNearPosition,
    PlayerHasBuff,
    PlayerHasSkill,
    PlayerHasClass,
    PlayerHasName,
    PlayerHasEnergy,
    KeyIsPressed,

    // Current target state
    CurrentTargetHasHpBelow,
    CurrentTargetIsUsingSkill,
    CurrentTargetHasModel,
    CurrentTargetAllegiance,

    NearbyAgent,

    Count
};

class Condition {
public:
    virtual ~Condition() {}
    virtual ConditionType type() const = 0;
    virtual bool check() const { return true; };
    virtual void drawSettings() {}
    virtual void serialize(std::ostringstream& stream) const { stream << "C " << (int)type() << " "; }
};
