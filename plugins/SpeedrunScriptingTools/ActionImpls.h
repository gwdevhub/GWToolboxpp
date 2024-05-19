#pragma once

#include <utils.h>
#include <Action.h>
#include <Condition.h>

#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/Constants/ItemIDs.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Utilities/Hook.h>

#include <chrono>
#include <unordered_set>

namespace GW {
    struct AgentLiving;
}

class MoveToAction : public Action {
public:
    MoveToAction();
    MoveToAction(InputStream&);
    ActionType type() const final { return ActionType::MoveTo; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    GW::GamePos pos{};
    float accuracy = GW::Constants::Range::Adjacent;
    bool repeatMove{true};
    mutable std::chrono::steady_clock::time_point lastMovePacketTime = std::chrono::steady_clock::now();
    mutable bool hasBegunWalking = false;
};

class CastOnSelfAction : public Action {
public:
    CastOnSelfAction() = default;
    CastOnSelfAction(InputStream&);
    ActionType type() const final { return ActionType::CastOnSelf; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill;
    mutable bool hasBegunCasting = false;
    mutable bool hasSkillReady = false;
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
};

class CastAction : public Action {
public:
    CastAction() = default;
    CastAction(InputStream&);
    ActionType type() const final { return ActionType::Cast; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill;
    bool hasSkillReady = false;
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
    mutable bool hasBegunCasting = false;
};

class CastBySlotAction : public Action {
public:
    CastBySlotAction() = default;
    CastBySlotAction(InputStream&);
    ActionType type() const final { return ActionType::CastBySlot; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    int slot = 1;
    bool hasSkillReady = false;
    GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill;
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
    mutable bool hasBegunCasting = false;
};

class ChangeTargetAction : public Action {
public:
    ChangeTargetAction() = default;
    ChangeTargetAction(InputStream&);
    ActionType type() const final { return ActionType::ChangeTarget; }
    void initialAction() final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    AgentType agentType = AgentType::Any;
    Class primary = Class::Any;
    Class secondary = Class::Any;
    Status status = Status::Alive;
    GW::Constants::SkillID skill = GW::Constants::SkillID::No_Skill;
    Sorting sorting = Sorting::AgentId;
    int modelId = 0;
    float minDistance = 0.f;
    float maxDistance = 5000.f;
    bool preferNonHexed = false;
    bool requireSameModelIdAsTarget = false;
    std::string agentName = "";
    std::vector<GW::Vec2f> polygon;
    bool rotateThroughTargets = true;
    std::unordered_set<GW::AgentID> recentlyTargetedEnemies;
    float minHp = 0.f;
    float maxHp = 100.f;
};

class UseItemAction : public Action {
public:
    UseItemAction() = default;
    UseItemAction(InputStream&);
    ActionType type() const final { return ActionType::UseItem; }
    void initialAction() final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    int id = 0;
};

class EquipItemAction : public Action {
public:
    EquipItemAction() = default;
    EquipItemAction(InputStream&);
    ActionType type() const final { return ActionType::EquipItem; }
    void initialAction() final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    int id = 0;
};

class SendDialogAction : public Action {
public:
    SendDialogAction() = default;
    SendDialogAction(InputStream&);
    ActionType type() const final { return ActionType::SendDialog; }
    void initialAction() final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    int id = 0;
};

class GoToTargetAction : public Action {
public:
    GoToTargetAction() = default;
    GoToTargetAction(InputStream&) : GoToTargetAction(){};
    ActionType type() const final { return ActionType::GoToTarget; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;

private:
    const GW::AgentLiving* target = nullptr;
};

class WaitAction : public Action {
public:
    WaitAction() = default;
    WaitAction(InputStream&);
    ActionType type() const final { return ActionType::Wait; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    int waitTime = 1000;
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
};

class SendChatAction : public Action {
public:
    SendChatAction() = default;
    SendChatAction(InputStream&);
    ActionType type() const final { return ActionType::SendChat; }
    void initialAction() final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    Channel channel = Channel::All;
    std::string message;
};

class CancelAction : public Action {
public:
    CancelAction() = default;
    CancelAction(InputStream&){}
    ActionType type() const final { return ActionType::Cancel; }
    void initialAction() final;
    void drawSettings() final;
};

class DropBuffAction : public Action {
public:
    DropBuffAction() = default;
    DropBuffAction(InputStream&);
    ActionType type() const final { return ActionType::DropBuff; }
    void initialAction() final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill;
};

class ConditionedAction : public Action {
public:
    ConditionedAction() = default;
    ConditionedAction(InputStream&);
    ActionType type() const final { return ActionType::Conditioned; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    std::shared_ptr<Condition> cond = nullptr;
    std::vector<std::shared_ptr<Action>> actionsTrue = {};
    std::vector<std::shared_ptr<Action>> actionsFalse = {};
    mutable std::vector<std::shared_ptr<Action>> currentlyExecutedActions = {};
};

class RepopMinipetAction : public Action {
public:
    RepopMinipetAction() = default;
    RepopMinipetAction(InputStream&);
    ActionType type() const final { return ActionType::RepopMinipet; }
    void initialAction() final;
    bool isComplete() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    int itemModelId = 36651;
    int agentModelId = 350;

    mutable GW::HookEntry hook;
    mutable bool agentHasSpawned = false;
    mutable bool hasUsedItem = false;
};

class PingHardModeAction : public Action {
public:
    PingHardModeAction() = default;
    PingHardModeAction(InputStream&) : PingHardModeAction(){}
    ActionType type() const final { return ActionType::PingHardMode; }
    void initialAction() final;
    void drawSettings() final;
};

class PingTargetAction : public Action {
public:
    PingTargetAction() = default;
    PingTargetAction(InputStream&) : PingTargetAction() {}
    ActionType type() const final { return ActionType::PingTarget; }
    void initialAction() final;
    void drawSettings() final;

private:
    bool onlyOncePerInstance = true;
    int instanceId = 0;
    std::unordered_set<GW::AgentID> pingedTargets;
};

class AutoAttackTargetAction : public Action {
public:
    AutoAttackTargetAction() = default;
    AutoAttackTargetAction(InputStream&) : AutoAttackTargetAction() {}
    ActionType type() const final { return ActionType::AutoAttackTarget; }
    void initialAction() final;
    void drawSettings() final;
};

class ChangeWeaponSetAction : public Action {
public:
    ChangeWeaponSetAction() = default;
    ChangeWeaponSetAction(InputStream&);
    ActionType type() const final { return ActionType::ChangeWeaponSet; }
    void initialAction() final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    int id = 1;
};

class StoreTargetAction : public Action {
public:
    StoreTargetAction() = default;
    StoreTargetAction(InputStream&);
    ActionType type() const final { return ActionType::StoreTarget; }
    void initialAction() final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    int id = 0;
};

class RestoreTargetAction : public Action {
public:
    RestoreTargetAction() = default;
    RestoreTargetAction(InputStream&);
    ActionType type() const final { return ActionType::RestoreTarget; }
    void initialAction() final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    int id = 0;
};
