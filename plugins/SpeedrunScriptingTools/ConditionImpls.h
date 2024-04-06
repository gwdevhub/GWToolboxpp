#pragma once

#include <Condition.h>

#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>

#include <cstdint>
#include <sstream>

enum class QuestStatus : int { NotStarted, Started, Completed, Failed };

class NegatedCondition : public Condition {
public:
    NegatedCondition() = default;
    NegatedCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::Not; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    std::shared_ptr<Condition> cond = nullptr;
};

class DisjunctionCondition : public Condition {
public:
    DisjunctionCondition() = default;
    DisjunctionCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::Or; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    std::shared_ptr<Condition> first = nullptr;
    std::shared_ptr<Condition> second = nullptr;
};

class IsInMapCondition : public Condition {
public:
    IsInMapCondition() = default;
    IsInMapCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::IsInMap; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    GW::Constants::MapID id = GW::Constants::MapID::The_Underworld;
};

class QuestHasStateCondition : public Condition {
public:
    QuestHasStateCondition() = default;
    QuestHasStateCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::QuestHasState; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    GW::Constants::QuestID id = GW::Constants::QuestID::None;
    QuestStatus status = QuestStatus::NotStarted;
};

class PartyPlayerCountCondition : public Condition {
public:
    PartyPlayerCountCondition() = default;
    PartyPlayerCountCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::PartyPlayerCount; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    int count = 1;
};

class InstanceProgressCondition : public Condition {
public:
    InstanceProgressCondition() = default;
    InstanceProgressCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::InstanceProgress; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    float requiredProgress = .5f;
};

class PlayerIsNearPositionCondition : public Condition {
public:
    PlayerIsNearPositionCondition() = default;
    PlayerIsNearPositionCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::PlayerIsNearPosition; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    GW::GamePos pos = {};
    float accuracy = GW::Constants::Range::Adjacent;
};

class PlayerHasBuffCondition : public Condition {
public:
    PlayerHasBuffCondition() = default;
    PlayerHasBuffCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::PlayerHasBuff; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill;
};

class PlayerHasSkillCondition : public Condition {
public:
    PlayerHasSkillCondition() = default;
    PlayerHasSkillCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::PlayerHasSkill; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill;
};

class CurrentTargetIsCastingSkillCondition : public Condition {
public:
    CurrentTargetIsCastingSkillCondition() = default;
    CurrentTargetIsCastingSkillCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::CurrentTargetIsUsingSkill; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill;
};
