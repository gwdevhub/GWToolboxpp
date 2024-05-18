#pragma once

#include <utils.h>

enum class ActionType : int {
    MoveTo,
    CastOnSelf,
    Cast, 
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
    Count
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
    virtual bool isComplete() const { return true; }
    virtual void drawSettings() = 0;
    virtual void serialize(OutputStream& stream) const { stream << "A " << type();}
    bool hasBeenStarted() const { return m_hasBeenStarted; }

protected:
    int drawId() const { return m_drawId; }

private:
    int m_drawId = 0;
    bool m_hasBeenStarted{false};
};
