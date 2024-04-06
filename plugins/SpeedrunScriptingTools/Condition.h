#pragma once

#include <sstream>

enum class ConditionType {
    // Logic Operators
    Not,
    Or,

    // Instance state
    IsInMap,
    QuestHasState,
    PartyPlayerCount,
    InstanceProgress,
    HasPartyWindowAllyOfName,
    OnlyTriggerOncePerInstance,

    // Player state
    PlayerIsNearPosition,
    PlayerHasBuff,
    PlayerHasSkill,
    PlayerHasClass,
    PlayerHasName,

    // Current target state
    CurrentTargetHasHpBelow,
    CurrentTargetIsUsingSkill,

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
