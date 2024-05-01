#pragma once

#include <sstream>

enum class ActionType : int {
    MoveTo,
    CastOnSelf,
    Cast,
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
    Count
};

class Action {
public:
    virtual ~Action(){};
    virtual ActionType type() const = 0;
    virtual void initialAction() { m_hasBeenStarted = true; }
    virtual void finalAction() { m_hasBeenStarted = false; }
    virtual bool isComplete() const { return true; }
    virtual void drawSettings() = 0;
    virtual void serialize(std::ostringstream& stream) const { stream << "A " << (int)type() << " ";}
    bool hasBeenStarted() const { return m_hasBeenStarted; }

private:
    bool m_hasBeenStarted{false};
};
