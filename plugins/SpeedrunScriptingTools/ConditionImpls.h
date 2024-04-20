#pragma once

#include <Condition.h>
#include <InstanceInfo.h>
#include <utils.h>

#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/GameContainers/GamePos.h>

#include <cstdint>
#include <sstream>

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

class OnlyTriggerOnceCondition : public Condition {
public:
    OnlyTriggerOnceCondition() = default;
    OnlyTriggerOnceCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::OnlyTriggerOncePerInstance; }
    bool check() const final;
    void drawSettings() final;

private:
    mutable int triggeredLastInInstanceId = 0;
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
    int minimumDuration = 0;
    int maximumDuration = 0;
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

class PlayerHasClassCondition : public Condition {
public:
    PlayerHasClassCondition() = default;
    PlayerHasClassCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::PlayerHasClass; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    Class primary = Class::Any;
    Class secondary = Class::Any;
};

class PlayerHasEnergyCondition : public Condition {
public:
    PlayerHasEnergyCondition() = default;
    PlayerHasEnergyCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::PlayerHasEnergy; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    int minEnergy = 0;
};


class PlayerHasNameCondition : public Condition {
public:
    PlayerHasNameCondition() = default;
    PlayerHasNameCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::PlayerHasName; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    std::string name;
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

class CurrentTargetHasHpBelowCondition : public Condition {
public:
    CurrentTargetHasHpBelowCondition() = default;
    CurrentTargetHasHpBelowCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::CurrentTargetHasHpBelow; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    float hp = 50.f;
};

class HasPartyWindowAllyOfNameCondition : public Condition {
public:
    HasPartyWindowAllyOfNameCondition() = default;
    HasPartyWindowAllyOfNameCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::HasPartyWindowAllyOfName; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    std::string name = "";
};

class PartyMemberStatusCondition : public Condition {
public:
    PartyMemberStatusCondition() = default;
    PartyMemberStatusCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::PartyMemberStatus; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    std::string name = "";
    Status status = Status::Dead;
};

class KeyIsPressedCondition : public Condition {
public:
    KeyIsPressedCondition() = default;
    KeyIsPressedCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::KeyIsPressed; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    long shortcutKey = 0;
    long shortcutMod = 0;
    std::string description = "Click to change key";
};

class InstanceTimeCondition : public Condition {
public:
    InstanceTimeCondition() = default;
    InstanceTimeCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::InstanceTime; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    int timeInSeconds = 0;
};

class NearbyAgentCondition : public Condition {
public:
    NearbyAgentCondition() = default;
    NearbyAgentCondition(std::istringstream&);
    ConditionType type() const final { return ConditionType::NearbyAgent; }
    bool check() const final;
    void drawSettings() final;
    void serialize(std::ostringstream&) const final;

private:
    AgentType agentType = AgentType::Any;
    Class primary = Class::Any;
    Class secondary = Class::Any;
    Status status = Status::Alive;
    HexedStatus hexed = HexedStatus::Any;
    GW::Constants::SkillID skill = GW::Constants::SkillID::No_Skill;
    int modelId = 0;
    float minDistance = 0.f;
    float maxDistance = 5000.f;
    std::string agentName = "";
};

class CanPopAgentCondition : public Condition {
public:
    CanPopAgentCondition() = default;
    CanPopAgentCondition(std::istringstream&) {}
    ConditionType type() const final { return ConditionType::CanPopAgent; }
    bool check() const final;
    void drawSettings() final;
};
