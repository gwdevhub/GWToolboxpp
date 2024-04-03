#pragma once

#include <Condition.h>

#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/GameContainers/GamePos.h>

#include <cstdint>

class IsInMapCondition : public Condition {
public:
    IsInMapCondition(GW::Constants::MapID id) : id{id} {}
    ConditionType type() const final { return ConditionType::IsInMap; }
    bool check() const final;

private:
    GW::Constants::MapID id;
};

class QuestHasStateCondition : public Condition {
public:
    // todo add constructor
    ConditionType type() const final { return ConditionType::QuestHasState; }
    bool check() const final;
};

class PartyPlayerCountCondition : public Condition {
public:
    PartyPlayerCountCondition(uint8_t count) : count{count} {}
    ConditionType type() const final { return ConditionType::PartyPlayerCount; }
    bool check() const final;

private:
    uint8_t count;
};

class InstanceProgressCondition : public Condition {
public:
    InstanceProgressCondition(float requiredProgress) : requiredProgress{requiredProgress} {}
    ConditionType type() const final { return ConditionType::InstanceProgress; }
    bool check() const final;

private:
    float requiredProgress;
};

class PlayerIsNearPositionCondition : public Condition {
public:
    PlayerIsNearPositionCondition(GW::GamePos pos, float squareAccuracy) : pos{pos}, squareAccuracy{squareAccuracy} {}
    ConditionType type() const final { return ConditionType::PlayerIsNearPosition; }
    bool check() const final;

private:
    GW::GamePos pos;
    float squareAccuracy;
};

class PlayerHasBuffCondition : public Condition {
public:
    PlayerHasBuffCondition(GW::Constants::SkillID id) : id{id} {}
    ConditionType type() const final { return ConditionType::PlayerHasBuff; }
    bool check() const final;

private:
    GW::Constants::SkillID id;
};

class PlayerHasSkillCondition : public Condition {
public:
    PlayerHasSkillCondition(GW::Constants::SkillID id) : id{id} {}
    ConditionType type() const final { return ConditionType::PlayerHasSkill; }
    bool check() const final;

private:
    GW::Constants::SkillID id;
};

class CurrentTargetIsCastingSkillCondition : public Condition {
public:
    CurrentTargetIsCastingSkillCondition(GW::Constants::SkillID id) : id{id} {}
    ConditionType type() const final { return ConditionType::CurrentTargetIsUsingSkill; }
    bool check() const final;

private:
    GW::Constants::SkillID id;
};
