#pragma once

#include <utils.h>

enum class ConditionType : int {
    // Logic Operators
    Not,
    Or,
    And,

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
    PlayerIsIdle,
    PlayerHasItemEquipped,
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
    Condition() { 
        static int counter = 0;
        m_drawId = counter++;
    }
    virtual ~Condition() {}
    virtual ConditionType type() const = 0;
    virtual bool check() const { return true; };
    virtual void drawSettings() {}
    virtual void serialize(OutputStream& stream) const { stream << "C" << type(); }

protected:
    int drawId() const { return m_drawId; }

private:
    int m_drawId = 0;
};
