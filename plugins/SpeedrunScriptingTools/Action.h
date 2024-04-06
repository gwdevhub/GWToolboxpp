#pragma once

#include <sstream>

enum class ActionType {
    MoveTo,
    CastOnSelf,
    CastOnTarget,
    UseItem,
    SendDialog,
    GoToNpc,
    Wait,
    SendChat,
    Cancel,
    Count
};

class Action {
public:
    virtual ~Action(){};
    virtual ActionType type() const = 0;
    virtual void initialAction() { m_hasBeenStarted = true; }
    virtual void finalAction() { m_hasBeenStarted = false; }
    virtual bool isComplete() const { return true; }
    virtual void drawSettings() {}
    virtual void serialize(std::ostringstream& stream) const { stream << "A " << (int)type() << " ";}
    bool hasBeenStarted() const { return m_hasBeenStarted; }

private:
    bool m_hasBeenStarted{false};
};
