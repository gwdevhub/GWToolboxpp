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

    // Player state
    PlayerIsNearPosition,
    PlayerHasBuff,
    PlayerHasSkill,

    // Current target state
    CurrentTargetHasHpPercentBelow,
    CurrentTargetIsUsingSkill,

    // All agents state
    NearbyAllyOfModelIdExists,
    NearbyEnemyWithModelIdCastingSpellExists,
    EnemyWithModelIdAndEffectExists,
    EnemyInPolygonWithModelIdExists,
    EnemyInCircleSegmentWithModelIdExísts,

    Count
};

class Condition {
public:
    virtual ~Condition() {}
    virtual ConditionType type() const = 0;
    virtual bool check() const { return true; };
    virtual void drawSettings() {}
    virtual void serialize(std::ostringstream& stream) const { 
        stream << "C " << (int)type() << " ";
    }
};
