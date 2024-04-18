#pragma once

#include <utils.h>
#include <Action.h>
#include <Condition.h>

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
    mutable bool hasBegunCasting = false;
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
    mutable bool hasBegunCasting = false;
};

class ChangeTargetAction : public Action {
public:
    ChangeTargetAction() = default;
    ChangeTargetAction(std::istringstream&);
    ActionType type() const final { return ActionType::ChangeTarget; }
    void initialAction() final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    AgentType agentType = AgentType::Any;
    Class primary = Class::Any;
    Class secondary = Class::Any;
    Status status = Status::Alive;
    HexedStatus hexed = HexedStatus::Any;
    GW::Constants::SkillID skill = GW::Constants::SkillID::No_Skill;
    Sorting sorting = Sorting::AgentId;
    int modelId = 0;
    float minDistance = 0.f;
    float maxDistance = 5000.f;
    bool mayBeCurrentTarget = true;
    bool requireSameModelIdAsTarget = false;
    std::string agentName = "";
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

class EquipItemAction : public Action {
public:
    EquipItemAction() = default;
    EquipItemAction(std::istringstream&);
    ActionType type() const final { return ActionType::EquipItem; }
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
    std::string message;
};

class CancelAction : public Action {
public:
    CancelAction() = default;
    CancelAction(std::istringstream&){}
    ActionType type() const final { return ActionType::Cancel; }
    void initialAction() final;
    void drawSettings() final;
};

class DropBuffAction : public Action {
public:
    DropBuffAction() = default;
    DropBuffAction(std::istringstream&);
    ActionType type() const final { return ActionType::DropBuff; }
    void initialAction() final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill;
};

class ConditionedAction : public Action {
public:
    ConditionedAction() = default;
    ConditionedAction(std::istringstream&);
    ActionType type() const final { return ActionType::Conditioned; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    std::shared_ptr<Condition> cond = nullptr;
    std::shared_ptr<Action> act = nullptr;
};

class RepopMinipetAction : public Action {
public:
    RepopMinipetAction() = default;
    RepopMinipetAction(std::istringstream&);
    ActionType type() const final { return ActionType::RepopMinipet; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    int id = 0;
};

class PingHardModeAction : public Action {
public:
    PingHardModeAction() = default;
    PingHardModeAction(std::istringstream&) : PingHardModeAction(){}
    ActionType type() const final { return ActionType::PingHardMode; }
    void initialAction() final;
    void drawSettings() final;
};
