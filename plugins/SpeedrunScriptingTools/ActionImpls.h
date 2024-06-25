#pragma once

#include <io.h>
#include <Enums.h>
#include <Action.h>
#include <Condition.h>
#include <commonIncludes.h>

#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Utilities/Hook.h>

#include <chrono>
#include <unordered_set>

class MoveToAction : public Action {
public:
    MoveToAction();
    MoveToAction(InputStream&);
    ActionType type() const final { return ActionType::MoveTo; }
    void initialAction() final;
    ActionStatus isComplete() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::CanBeRunInOutpost; }

private:
    GW::GamePos pos{};
    float accuracy = GW::Constants::Range::Adjacent;
    MoveToBehaviour moveBehaviour = MoveToBehaviour::RepeatIfIdle;
    mutable std::chrono::steady_clock::time_point lastMovePacketTime = std::chrono::steady_clock::now();
    mutable bool hasBegunWalking = false;
};

class MoveToTargetPositionAction : public Action {
public:
    MoveToTargetPositionAction() = default;
    MoveToTargetPositionAction(InputStream&);
    ActionType type() const final { return ActionType::MoveToTargetPosition; }
    void initialAction() final;
    ActionStatus isComplete() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::CanBeRunInOutpost; }

private:
    float accuracy = GW::Constants::Range::Adjacent;
    MoveToBehaviour moveBehaviour = MoveToBehaviour::RepeatIfIdle;

    GW::GamePos pos{}; // Set in initial action
    mutable std::chrono::steady_clock::time_point lastMovePacketTime = std::chrono::steady_clock::now();
    mutable bool hasBegunWalking = false;
    mutable bool hasTarget = false;
};

class CastAction : public Action {
public:
    CastAction() = default;
    CastAction(InputStream&);
    ActionType type() const final { return ActionType::Cast; }
    void initialAction() final;
    ActionStatus isComplete() const final;
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
    ActionStatus isComplete() const final;
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
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::CanBeRunInOutpost; }

private:
    AgentType agentType = AgentType::Any;
    Class primary = Class::Any;
    Class secondary = Class::Any;
    AnyNoYes alive = AnyNoYes::Yes;
    AnyNoYes bleeding = AnyNoYes::Any;
    AnyNoYes poisoned = AnyNoYes::Any;
    AnyNoYes weaponspelled = AnyNoYes::Any;
    AnyNoYes enchanted = AnyNoYes::Any;
    AnyNoYes hexed = AnyNoYes::Any;
    GW::Constants::SkillID skill = GW::Constants::SkillID::No_Skill;
    Sorting sorting = Sorting::AgentId;
    uint16_t modelId = 0;
    float minDistance = 0.f;
    float maxDistance = 5000.f;
    bool preferNonHexed = false;
    bool requireSameModelIdAsTarget = false;
    std::string agentName = "";
    std::vector<GW::Vec2f> polygon;
    bool rotateThroughTargets = false;
    std::unordered_set<GW::AgentID> recentlyTargetedEnemies;
    float minHp = 0.f;
    float maxHp = 100.f;
    float minAngle = 0.f;
    float maxAngle = 180.f;
};

class UseItemAction : public Action {
public:
    UseItemAction() = default;
    UseItemAction(InputStream&);
    ActionType type() const final { return ActionType::UseItem; }
    void initialAction() final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::ImmediateFinish; }

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
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::ImmediateFinish | ActionBehaviourFlag::CanBeRunInOutpost; }

private:
    int id = 0;
    int modstruct = 0;
    bool hasModstruct = false;
};

class UnequipItemAction : public Action {
public:
    UnequipItemAction() = default;
    UnequipItemAction(InputStream&);
    ActionType type() const final { return ActionType::UnequipItem; }
    void initialAction() final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::ImmediateFinish | ActionBehaviourFlag::CanBeRunInOutpost; }

private:
    EquippedItemSlot slot = EquippedItemSlot::Mainhand;
};

class SendDialogAction : public Action {
public:
    SendDialogAction() = default;
    SendDialogAction(InputStream&);
    ActionType type() const final { return ActionType::SendDialog; }
    void initialAction() final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::ImmediateFinish | ActionBehaviourFlag::CanBeRunInOutpost; }

private:
    int id = 0;
};

class GoToTargetAction : public Action {
public:
    GoToTargetAction() = default;
    GoToTargetAction(InputStream&);
    ActionType type() const final { return ActionType::GoToTarget; }
    void initialAction() final;
    void finalAction() final;
    ActionStatus isComplete() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::CanBeRunInOutpost; }

private:
    GoToTargetFinishCondition finishCondition = GoToTargetFinishCondition::StoppedMovingNextToTarget;

    const GW::AgentLiving* target = nullptr;
    mutable GW::HookEntry hook;
    mutable bool dialogHasPoppedUp = false;
};

class WaitAction : public Action {
public:
    WaitAction() = default;
    WaitAction(InputStream&);
    ActionType type() const final { return ActionType::Wait; }
    void initialAction() final;
    ActionStatus isComplete() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::CanBeRunInOutpost; }

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
    ActionBehaviourFlags behaviour() const final;

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
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::CanBeRunInOutpost | ActionBehaviourFlag::ImmediateFinish; }
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
    ActionStatus isComplete() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;
    ActionBehaviourFlags behaviour() const final;

private:
    std::shared_ptr<Condition> cond = nullptr;
    std::vector<std::shared_ptr<Action>> actionsIf = {};
    std::vector<std::shared_ptr<Action>> actionsElse = {};
    std::vector<std::pair<std::shared_ptr<Condition>, std::vector<std::shared_ptr<Action>>>> actionsElseIf = {};
    mutable std::vector<std::shared_ptr<Action>> currentlyExecutedActions = {};
};

class RepopMinipetAction : public Action {
public:
    RepopMinipetAction() = default;
    RepopMinipetAction(InputStream&);
    ActionType type() const final { return ActionType::RepopMinipet; }
    void initialAction() final;
    void finalAction() final;
    ActionStatus isComplete() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::CanBeRunInOutpost; }

private:
    int itemModelId = 36651;
    uint16_t agentModelId = 350;

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
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::ImmediateFinish | ActionBehaviourFlag::CanBeRunInOutpost; }
};

class PingTargetAction : public Action {
public:
    PingTargetAction() = default;
    PingTargetAction(InputStream&) : PingTargetAction() {}
    ActionType type() const final { return ActionType::PingTarget; }
    void initialAction() final;
    void drawSettings() final;
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::ImmediateFinish | ActionBehaviourFlag::CanBeRunInOutpost; }

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
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::CanBeRunInOutpost; }

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
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::ImmediateFinish | ActionBehaviourFlag::CanBeRunInOutpost; }

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
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::ImmediateFinish | ActionBehaviourFlag::CanBeRunInOutpost; }

private:
    int id = 0;
};

class StopScriptAction : public Action {
public:
    StopScriptAction() = default;
    StopScriptAction(InputStream&){}
    ActionType type() const final { return ActionType::StopScript; }
    ActionStatus isComplete() const final { return ActionStatus::Error; }
    void drawSettings() final;
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::CanBeRunInOutpost; }
};

class LogOutAction : public Action {
public:
    LogOutAction() = default;
    LogOutAction(InputStream&) {}
    ActionType type() const final { return ActionType::LogOut; }
    void initialAction() final;
    void drawSettings() final;
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::CanBeRunInOutpost; }
};

class UseHeroSkillAction : public Action {
public:
    UseHeroSkillAction() = default;
    UseHeroSkillAction(InputStream&);
    ActionType type() const final { return ActionType::UseHeroSkill; }
    void initialAction() final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    GW::Constants::HeroID hero = GW::Constants::HeroID::NoHero;
    GW::Constants::SkillID skill = GW::Constants::SkillID::No_Skill;
};

class ClearTargetAction : public Action {
public:
    ClearTargetAction() = default;
    ClearTargetAction(InputStream&) {}
    ActionType type() const final { return ActionType::ClearTarget; }
    void initialAction() final;
    void drawSettings() final;
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::CanBeRunInOutpost | ActionBehaviourFlag::ImmediateFinish; }
};

class WaitUntilAction : public Action {
public:
    WaitUntilAction() = default;
    WaitUntilAction(InputStream&);
    ActionType type() const final { return ActionType::WaitUntil; }
    ActionStatus isComplete() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;
    ActionBehaviourFlags behaviour() const final { return ActionBehaviourFlag::CanBeRunInOutpost; }

private:
    std::shared_ptr<Condition> condition = nullptr;
};
