#pragma once

#include <Condition.h>
#include <Enums.h>

#include <commonIncludes.h>
#include <GWCA/Constants/QuestIDs.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>

class NegatedCondition : public Condition {
public:
    NegatedCondition() = default;
    NegatedCondition(InputStream&);
    ConditionType type() const final { return ConditionType::Not; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    std::shared_ptr<Condition> cond = nullptr;
};

class DisjunctionCondition : public Condition {
public:
    DisjunctionCondition() = default;
    DisjunctionCondition(InputStream&);
    ConditionType type() const final { return ConditionType::Or; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    std::vector<std::shared_ptr<Condition>> conditions{};
};

class ConjunctionCondition : public Condition {
public:
    ConjunctionCondition() = default;
    ConjunctionCondition(InputStream&);
    ConditionType type() const final { return ConditionType::And; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    std::vector<std::shared_ptr<Condition>> conditions{};
};

class IsInMapCondition : public Condition {
public:
    IsInMapCondition();
    IsInMapCondition(InputStream&);
    ConditionType type() const final { return ConditionType::IsInMap; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    GW::Constants::MapID id = GW::Constants::MapID::The_Underworld;
};

class QuestHasStateCondition : public Condition {
public:
    QuestHasStateCondition() = default;
    QuestHasStateCondition(InputStream&);
    ConditionType type() const final { return ConditionType::QuestHasState; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    GW::Constants::QuestID id = GW::Constants::QuestID::None;
    QuestStatus status = QuestStatus::NotStarted;
};

class PartyPlayerCountCondition : public Condition {
public:
    PartyPlayerCountCondition() = default;
    PartyPlayerCountCondition(InputStream&);
    ConditionType type() const final { return ConditionType::PartyPlayerCount; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    int count = 1;
};

class InstanceProgressCondition : public Condition {
public:
    InstanceProgressCondition() = default;
    InstanceProgressCondition(InputStream&);
    ConditionType type() const final { return ConditionType::InstanceProgress; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    float requiredProgress = 50.f;
};

class OnlyTriggerOnceCondition : public Condition {
public:
    OnlyTriggerOnceCondition() = default;
    OnlyTriggerOnceCondition(InputStream&);
    ConditionType type() const final { return ConditionType::OnlyTriggerOncePerInstance; }
    bool check() const final;
    void drawSettings() final;

private:
    mutable int triggeredLastInInstanceId = 0;
};

class PlayerIsNearPositionCondition : public Condition {
public:
    PlayerIsNearPositionCondition();
    PlayerIsNearPositionCondition(InputStream&);
    ConditionType type() const final { return ConditionType::PlayerIsNearPosition; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    GW::GamePos pos = {};
    float accuracy = GW::Constants::Range::Adjacent;
};

class PlayerHasBuffCondition : public Condition {
public:
    PlayerHasBuffCondition() = default;
    PlayerHasBuffCondition(InputStream&);
    ConditionType type() const final { return ConditionType::PlayerHasBuff; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill;
    int minimumDuration = 0;
    int maximumDuration = 0;
    bool hasMinimumDuration = false;
    bool hasMaximumDuration = false;
};

class PlayerHasSkillCondition : public Condition {
public:
    PlayerHasSkillCondition() = default;
    PlayerHasSkillCondition(InputStream&);
    ConditionType type() const final { return ConditionType::PlayerHasSkill; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill;
};

class PlayerHasClassCondition : public Condition {
public:
    PlayerHasClassCondition() = default;
    PlayerHasClassCondition(InputStream&);
    ConditionType type() const final { return ConditionType::PlayerHasClass; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    Class primary = Class::Any;
    Class secondary = Class::Any;
};

class PlayerHasEnergyCondition : public Condition {
public:
    PlayerHasEnergyCondition() = default;
    PlayerHasEnergyCondition(InputStream&);
    ConditionType type() const final { return ConditionType::PlayerHasEnergy; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    int minEnergy = 0;
};


class PlayerHasNameCondition : public Condition {
public:
    PlayerHasNameCondition() = default;
    PlayerHasNameCondition(InputStream&);
    ConditionType type() const final { return ConditionType::PlayerHasName; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    std::string name = "";
};

class CurrentTargetIsCastingSkillCondition : public Condition {
public:
    CurrentTargetIsCastingSkillCondition() = default;
    CurrentTargetIsCastingSkillCondition(InputStream&);
    ConditionType type() const final { return ConditionType::CurrentTargetIsUsingSkill; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill;
};

class CurrentTargetDistanceCondition : public Condition {
public:
    CurrentTargetDistanceCondition() = default;
    CurrentTargetDistanceCondition(InputStream&);
    ConditionType type() const final { return ConditionType::CurrentTargetDistance; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    float minDistance = 0.f;
    float maxDistance = 5000.f;
};

class CurrentTargetHasHpBelowCondition : public Condition {
public:
    CurrentTargetHasHpBelowCondition() = default;
    CurrentTargetHasHpBelowCondition(InputStream&);
    ConditionType type() const final { return ConditionType::CurrentTargetHasHpBelow; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    float hp = 50.f;
};

class CurrentTargetAllegianceCondition : public Condition {
public:
    CurrentTargetAllegianceCondition() = default;
    CurrentTargetAllegianceCondition(InputStream&);
    ConditionType type() const final { return ConditionType::CurrentTargetAllegiance; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    AgentType agentType = AgentType::Hostile;
};

class CurrentTargetModelCondition : public Condition {
public:
    CurrentTargetModelCondition() = default;
    CurrentTargetModelCondition(InputStream&);
    ConditionType type() const final { return ConditionType::CurrentTargetHasModel; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    int modelId = 0;
};

class HasPartyWindowAllyOfNameCondition : public Condition {
public:
    HasPartyWindowAllyOfNameCondition() = default;
    HasPartyWindowAllyOfNameCondition(InputStream&);
    ConditionType type() const final { return ConditionType::HasPartyWindowAllyOfName; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    std::string name = "";
};

class PartyMemberStatusCondition : public Condition {
public:
    PartyMemberStatusCondition() = default;
    PartyMemberStatusCondition(InputStream&);
    ConditionType type() const final { return ConditionType::PartyMemberStatus; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    std::string name = "";
    Status status = Status::Dead;
};

class KeyIsPressedCondition : public Condition {
public:
    KeyIsPressedCondition() = default;
    KeyIsPressedCondition(InputStream&);
    ConditionType type() const final { return ConditionType::KeyIsPressed; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    long shortcutKey = 0;
    long shortcutMod = 0;
    std::string description = "Click to change key";
};

class InstanceTimeCondition : public Condition {
public:
    InstanceTimeCondition() = default;
    InstanceTimeCondition(InputStream&);
    ConditionType type() const final { return ConditionType::InstanceTime; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    int timeInSeconds = 0;
};

class NearbyAgentCondition : public Condition {
public:
    NearbyAgentCondition() = default;
    NearbyAgentCondition(InputStream&);
    ConditionType type() const final { return ConditionType::NearbyAgent; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

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
    std::vector<GW::Vec2f> polygon;
    float minHp = 0.f;
    float maxHp = 100.f;
};

class CanPopAgentCondition : public Condition {
public:
    CanPopAgentCondition() = default;
    CanPopAgentCondition(InputStream&) {}
    ConditionType type() const final { return ConditionType::CanPopAgent; }
    bool check() const final;
    void drawSettings() final;
};

class PlayerIsIdleCondition : public Condition {
public:
    PlayerIsIdleCondition() = default;
    PlayerIsIdleCondition(InputStream&) {}
    ConditionType type() const final { return ConditionType::PlayerIsIdle; }
    bool check() const final;
    void drawSettings() final;
};

class PlayerHasItemEquippedCondition : public Condition {
public:
    PlayerHasItemEquippedCondition() = default;
    PlayerHasItemEquippedCondition(InputStream&);
    ConditionType type() const final { return ConditionType::PlayerHasItemEquipped; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    int modelId = 0;
};
