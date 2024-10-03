#pragma once

#include <Condition.h>
#include <Enums.h>

#include <commonIncludes.h>
#include <GWCA/Constants/QuestIDs.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>
#include <Characteristic.h>

#include <chrono>

class NegatedCondition : public Condition {
public:
    NegatedCondition() = default;
    NegatedCondition(InputStream&);
    ConditionType type() const final { return ConditionType::Not; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    ConditionPtr cond = nullptr;
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
    std::vector<ConditionPtr> conditions{};
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
    std::vector<ConditionPtr> conditions{};
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
    int id = 0;
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

class PartyHasLoadedInCondition : public Condition {
public:
    PartyHasLoadedInCondition() = default;
    PartyHasLoadedInCondition(InputStream&);
    ConditionType type() const final { return ConditionType::PartyHasLoadedIn; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    PlayerConnectednessRequirement req = PlayerConnectednessRequirement::All;
    int slot = 1;
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
    OnlyTriggerOnceCondition(InputStream&){}
    ConditionType type() const final { return ConditionType::OnlyTriggerOncePerInstance; }
    bool check() const final;
    void drawSettings() final;

private:
    mutable int triggeredLastInInstanceId = 0;
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
    HasSkillRequirement requirement = HasSkillRequirement::OffCooldown;
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
    AnyNoYes alive = AnyNoYes::No;
};

class KeyIsPressedCondition : public Condition {
public:
    KeyIsPressedCondition() = default;
    KeyIsPressedCondition(InputStream&);
    ~KeyIsPressedCondition();
    ConditionType type() const final { return ConditionType::KeyIsPressed; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    long shortcutKey = 0;
    long shortcutMod = 0;
    std::string description = "Click to change key";
    bool blockKey = false;
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

class CanPopAgentCondition : public Condition {
public:
    CanPopAgentCondition() = default;
    CanPopAgentCondition(InputStream&) {}
    ConditionType type() const final { return ConditionType::CanPopAgent; }
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

class ItemInInventoryCondition : public Condition {
public:
    ItemInInventoryCondition() = default;
    ItemInInventoryCondition(InputStream&);
    ConditionType type() const final { return ConditionType::ItemInInventory; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    int modelId = 0;
};

class InstanceTypeCondition : public Condition {
public:
    InstanceTypeCondition() = default;
    InstanceTypeCondition(InputStream&);
    ConditionType type() const final { return ConditionType::InstanceType; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    GW::Constants::InstanceType instanceType = GW::Constants::InstanceType::Explorable;
};

class RemainingCooldownCondition : public Condition {
public:
    RemainingCooldownCondition() = default;
    RemainingCooldownCondition(InputStream&);
    ConditionType type() const final { return ConditionType::RemainingCooldown; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    GW::Constants::SkillID id = GW::Constants::SkillID::No_Skill;
    bool hasMinimumCooldown = false;
    bool hasMaximumCooldown = false;
    int minimumCooldown = 0;
    int maximumCooldown = 1000;
};

class FoeCountCondition : public Condition {
public:
    FoeCountCondition() = default;
    FoeCountCondition(InputStream&);
    ConditionType type() const final { return ConditionType::FoeCount; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    int maximum = 0;
};

class MoraleCondition : public Condition {
public:
    MoraleCondition() = default;
    MoraleCondition(InputStream&);
    ConditionType type() const final { return ConditionType::PlayerMorale; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    int minimumMorale = 0;
};

class FalseCondition : public Condition {
public:
    FalseCondition() = default;
    FalseCondition(InputStream&) {}
    ConditionType type() const final { return ConditionType::False; }
    bool check() const final;
    void drawSettings() final;
};

class TrueCondition : public Condition {
public:
    TrueCondition() = default;
    TrueCondition(InputStream&) {}
    ConditionType type() const final { return ConditionType::True; }
    bool check() const final;
    void drawSettings() final;
};

class OnceCondition : public Condition {
public:
    OnceCondition() = default;
    OnceCondition(InputStream&);
    ConditionType type() const final { return ConditionType::Once; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    ConditionPtr cond = nullptr;
    mutable int triggeredLastInInstanceId = 0;
};

class UntilCondition : public Condition {
public:
    UntilCondition() = default;
    UntilCondition(InputStream&);
    ConditionType type() const final { return ConditionType::Until; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    ConditionPtr cond = nullptr;
    mutable int conditionLastTrueInInstanceId = 0;
    mutable bool currentState = true;
};

class AfterCondition : public Condition {
public:
    AfterCondition() = default;
    AfterCondition(InputStream&);
    ConditionType type() const final { return ConditionType::After; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    ConditionPtr cond = nullptr;
    mutable int conditionLastTrueInInstanceId = 0;
    mutable bool currentState = false;
};

class ToggleCondition : public Condition {
public:
    ToggleCondition() = default;
    ToggleCondition(InputStream&);
    ConditionType type() const final { return ConditionType::Toggle; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    TrueFalse defaultState = TrueFalse::False;
    ConditionPtr toggleOnCond = nullptr;
    ConditionPtr toggleOffCond = nullptr;
    mutable int lastResetInInstanceId = 0;
    mutable bool currentState = false;
};

class ThrottleCondition : public Condition {
public:
    ThrottleCondition() = default;
    ThrottleCondition(InputStream&);
    ConditionType type() const final { return ConditionType::Throttle; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    int delayInMs = 100;

    mutable std::chrono::steady_clock::time_point lastTimeReturnedTrue = std::chrono::steady_clock::now() - std::chrono::days{1};
};

class PlayerHasCharacteristicsCondition : public Condition {
public:
    PlayerHasCharacteristicsCondition() = default;
    PlayerHasCharacteristicsCondition(InputStream&);
    ConditionType type() const final { return ConditionType::PlayerHasCharacteristics; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    std::vector<CharacteristicPtr> characteristics;
};

class TargetHasCharacteristicsCondition : public Condition {
public:
    TargetHasCharacteristicsCondition() = default;
    TargetHasCharacteristicsCondition(InputStream&);
    ConditionType type() const final { return ConditionType::TargetHasCharacteristics; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    std::vector<CharacteristicPtr> characteristics;
};

class AgentWithCharacteristicsCountCondition : public Condition {
public:
    AgentWithCharacteristicsCountCondition() = default;
    AgentWithCharacteristicsCountCondition(InputStream&);
    ConditionType type() const final { return ConditionType::AgentWithCharacteristicsCount; }
    bool check() const final;
    void drawSettings() final;
    void serialize(OutputStream&) const final;

private:
    std::vector<CharacteristicPtr> characteristics;
    int count = 1;
    ComparisonOperator comp = ComparisonOperator::GreaterOrEqual;
};
