#pragma once

#include <io.h>

enum class ConditionType : int {
    Not,
    Or,
    And,
    IsInMap,
    QuestHasState,
    PartyPlayerCount,
    PartyMemberStatus,
    HasPartyWindowAllyOfName,
    InstanceProgress,
    InstanceTime,
    OnlyTriggerOncePerInstance,
    CanPopAgent,
    PlayerIsNearPosition,
    PlayerHasBuff,
    PlayerHasSkill,
    PlayerHasClass,
    PlayerHasName,
    PlayerHasEnergy,
    PlayerIsIdle,
    PlayerHasItemEquipped,
    KeyIsPressed,
    CurrentTargetHasHpBelow,
    CurrentTargetIsUsingSkill,
    CurrentTargetHasModel,
    CurrentTargetAllegiance,
    NearbyAgent,
    CurrentTargetDistance,
    PlayerHasHpBelow,
    PartyHasLoadedIn,
    ItemInInventory,
    PlayerStatus,
    CurrentTargetStatus,
    PlayerInPolygonCondition,

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
