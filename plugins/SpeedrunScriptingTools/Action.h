#pragma once

#include <io.h>
#include <Enums.h>

// This struct is append only, do NOT change the ordering of the values or add new ones at any place but the end
enum class ActionType : int {
    MoveTo,
    //CastOnSelf, Removed
    Cast = 2, 
    CastBySlot,
    DropBuff, 
    ChangeTarget,
    UseItem,
    EquipItem, 
    RepopMinipet, 
    PingHardMode,
    PingTarget,
    AutoAttackTarget,
    SendDialog,
    GoToTarget,
    Wait,
    SendChat,
    Cancel, 
    Conditioned,
    ChangeWeaponSet,
    StoreTarget,
    RestoreTarget,
    StopScript,
    LogOut,
    UseHeroSkill,
    UnequipItem,
    ClearTarget,
    WaitUntil,
    MoveToTargetPosition,

    Count
};

enum class ActionStatus {
    Running,
    Complete,
    Error
};

class Action {
public:
    Action()
    {
        static int counter = 0;
        m_drawId = counter++;
    }
    virtual ~Action(){};
    virtual ActionType type() const = 0;
    virtual void initialAction() { m_hasBeenStarted = true; }
    virtual void finalAction() { m_hasBeenStarted = false; }
    virtual ActionStatus isComplete() const { return ActionStatus::Complete; }
    virtual void drawSettings() = 0;
    virtual void serialize(OutputStream& stream) const { stream << "A" << type();}
    virtual ActionBehaviourFlags behaviour() const { return ActionBehaviourFlag::Default; }
    bool hasBeenStarted() const { return m_hasBeenStarted; }

protected:
    int drawId() const { return m_drawId; }

private:
    int m_drawId = 0;
    bool m_hasBeenStarted{false};
};
