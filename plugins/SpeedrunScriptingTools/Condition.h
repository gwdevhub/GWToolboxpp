#pragma once

enum class ConditionType {
    IsInMap,
    QuestHasState,
    PartyPlayerCount,
    InstanceProgress,
    HasPartyWindowAllyOfName,

    PlayerIsNearPosition,
    PlayerHasBuff,
    PlayerHasSkill,

    CurrentTargetHasHpPercentBelow,
    CurrentTargetIsUsingSkill,

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
};
