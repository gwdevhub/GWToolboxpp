#pragma once

enum class ActionType {
    MoveTo,
    CastOnSelf,
    CastOnTarget,
    UseItem,
    SendDialog,
    GoToNpc,
    Wait,
    SendChat,
};

class Action {
public:
    virtual ~Action(){};
    virtual ActionType type() const = 0;
    virtual void initialAction() { m_hasBeenStarted = true; }
    virtual void finalAction() { m_hasBeenStarted = false; }
    virtual bool isComplete() const { return true; }
    bool hasBeenStarted() const { return m_hasBeenStarted; }

protected:
    bool m_hasBeenStarted{false};
};
