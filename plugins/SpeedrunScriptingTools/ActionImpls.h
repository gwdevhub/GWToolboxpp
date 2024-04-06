#pragma once

#include <Action.h>

#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/Constants/ItemIDs.h>
#include <GWCA/Constants/Constants.h>

#include <chrono>
#include <sstream>

namespace GW {
    struct AgentLiving;
}

class MoveToAction : public Action {
public:
    MoveToAction() = default;
    MoveToAction(std::istringstream&);
    ActionType type() const final { return ActionType::MoveTo; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    GW::GamePos pos{};
    float accuracy = GW::Constants::Range::Adjacent;
};

class CastOnSelfAction : public Action {
public:
    CastOnSelfAction() = default;
    CastOnSelfAction(std::istringstream&);
    ActionType type() const final { return ActionType::CastOnSelf; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill;
};

class CastOnTargetAction : public Action {
public:
    CastOnTargetAction() = default;
    CastOnTargetAction(std::istringstream&);
    ActionType type() const final { return ActionType::CastOnTarget; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill;
};

class UseItemAction : public Action {
public:
    UseItemAction() = default;
    UseItemAction(std::istringstream&);
    ActionType type() const final { return ActionType::UseItem; }
    void initialAction() final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    int id = 0;
};

class SendDialogAction : public Action {
public:
    SendDialogAction() = default;
    SendDialogAction(std::istringstream&);
    ActionType type() const final { return ActionType::SendDialog; }
    void initialAction() final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    int id = 0;
};

class GoToNpcAction : public Action {
public:
    GoToNpcAction() = default;
    GoToNpcAction(std::istringstream&);
    ActionType type() const final { return ActionType::GoToNpc; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    int id = 0;
    float accuracy = GW::Constants::Range::Adjacent;
    const GW::AgentLiving* npc = nullptr;
};

class WaitAction : public Action {
public:
    WaitAction() = default;
    WaitAction(std::istringstream&);
    ActionType type() const final { return ActionType::Wait; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    int waitTime = 1000;
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
};

class SendChatAction : public Action {
public:
    enum class Channel {
        All,
        Guild,
        Team,
        Trade,
        Alliance,
        Whisper,
        Emote,
    };
    SendChatAction() = default;
    SendChatAction(std::istringstream&);
    ActionType type() const final { return ActionType::SendChat; }
    void initialAction() final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    Channel channel = Channel::All;
    char buf[100] = "";
};

class CancelAction : public Action {
public:
    CancelAction() = default;
    CancelAction(std::istringstream&){}
    ActionType type() const final { return ActionType::Cancel; }
    void initialAction() final;
    void drawSettings() final;
};
