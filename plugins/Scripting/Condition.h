#pragma once

#include <io.h>

struct Hotkey;

// This struct is append only, do NOT change the ordering of the values or add new ones at any place but the end
enum class ConditionType : int {
    Not,
    Or,
    And,
    IsInMap,
    Deprecated_QuestObjectiveHasState,
    PartyPlayerCount,
    PartyMemberStatus,
    HasPartyWindowAllyOfName,
    InstanceProgress,
    InstanceTime,
    OnlyTriggerOncePerInstance,
    CanPopAgent,
    PlayerHasBuff = 13,
    PlayerHasSkill,
    PlayerHasEnergy = 17,
    PlayerHasItemEquipped = 19,
    KeyIsPressed,
    PartyHasLoadedIn = 28,
    ItemInInventory,
    InstanceType = 33,
    RemainingCooldown,
    FoeCount,
    PlayerMorale,
    False,
    True,
    Until,
    Once,
    Toggle,
    After,
    Throttle = 44,
    PlayerHasCharacteristics = 46,
    TargetHasCharacteristics,
    AgentWithCharacteristicsCount,
    ScriptVariableValue,
    PlayerHasSkillBySlot,
    ScriptVariableIsSet,
    DoorStatus,
    PlayerHasEnergyRegen,
    QuestHasState,
    ObjectiveHasState,
    HeroHasSkill,

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

    // @return true iff disabledKey changed. Maybe TODO: Return true if any setting changed; currently not done because it's not used anywhere.
    virtual bool drawSettings() { return false;  }
    virtual void serialize(OutputStream& stream) const { stream << "C" << type(); }
    // @return Hotkey to block from sending it to GW
    virtual std::vector<Hotkey> disabledKeys() const { return {}; }

protected:
    int drawId() const { return m_drawId; }

private:
    int m_drawId = 0;
};

using ConditionPtr = std::shared_ptr<Condition>;
