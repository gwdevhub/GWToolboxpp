#pragma once

#include <Condition.h>

#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>

#include <cstdint>

class IsInMapCondition : public Condition {
public:
    IsInMapCondition(GW::Constants::MapID id = GW::Constants::MapID::The_Underworld) : id{id} {}
    ConditionType type() const final { return ConditionType::IsInMap; }
    bool check() const final;
    void drawSettings() final;

private:
    GW::Constants::MapID id;
};

class QuestHasStateCondition : public Condition {
public:
    // todo add constructor
    ConditionType type() const final { return ConditionType::QuestHasState; }
    bool check() const final;
    void drawSettings() final;
};

class PartyPlayerCountCondition : public Condition {
public:
    PartyPlayerCountCondition(int count = 8) : count{count} {}
    ConditionType type() const final { return ConditionType::PartyPlayerCount; }
    bool check() const final;
    void drawSettings() final;

private:
    int count;
};

class InstanceProgressCondition : public Condition {
public:
    InstanceProgressCondition(float requiredProgress = .0f) : requiredProgress{requiredProgress} {}
    ConditionType type() const final { return ConditionType::InstanceProgress; }
    bool check() const final;
    void drawSettings() final;

private:
    float requiredProgress;
};

class PlayerIsNearPositionCondition : public Condition {
public:
    PlayerIsNearPositionCondition(GW::GamePos pos = {}, float accuracy = GW::Constants::Range::Adjacent) : pos{pos}, accuracy{accuracy} {}
    ConditionType type() const final { return ConditionType::PlayerIsNearPosition; }
    bool check() const final;
    void drawSettings() final;

private:
    GW::GamePos pos;
    float accuracy;
};

class PlayerHasBuffCondition : public Condition {
public:
    PlayerHasBuffCondition(GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill) : id{id} {}
    ConditionType type() const final { return ConditionType::PlayerHasBuff; }
    bool check() const final;
    void drawSettings() final;

private:
    GW::Constants::SkillID id;
};

class PlayerHasSkillCondition : public Condition {
public:
    PlayerHasSkillCondition(GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill) : id{id} {}
    ConditionType type() const final { return ConditionType::PlayerHasSkill; }
    bool check() const final;
    void drawSettings() final;

private:
    GW::Constants::SkillID id;
};

class CurrentTargetIsCastingSkillCondition : public Condition {
public:
    CurrentTargetIsCastingSkillCondition(GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill) : id{id} {}
    ConditionType type() const final { return ConditionType::CurrentTargetIsUsingSkill; }
    bool check() const final;
    void drawSettings() final;

private:
    GW::Constants::SkillID id;
};
