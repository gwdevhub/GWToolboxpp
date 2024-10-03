#include <Characteristic.h>
#include <Enums.h>

#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Constants/Skills.h>

class PositionCharacteristic : public Characteristic 
{
public:
    PositionCharacteristic();
    PositionCharacteristic(InputStream&);
    void serialize(OutputStream&) const final;

    CharacteristicType type() const final { return CharacteristicType::Position; }
    bool check(const GW::AgentLiving& agent) const final;
    void drawSettings() final;

private:
    GW::Vec2f position{};
    ComparisonOperator comp = ComparisonOperator::LessOrEqual;
    float distance = 1248.f;
};

class PositionPolygonCharacteristic : public Characteristic {
public:
    PositionPolygonCharacteristic() = default;
    PositionPolygonCharacteristic(InputStream&);
    void serialize(OutputStream&) const final;

    CharacteristicType type() const final { return CharacteristicType::PositionPolygon; }
    bool check(const GW::AgentLiving& agent) const final;
    void drawSettings() final;

private:
    std::vector<GW::Vec2f> polygon{};
};

class DistanceToPlayerCharacteristic : public Characteristic {
public:
    DistanceToPlayerCharacteristic() = default;
    DistanceToPlayerCharacteristic(InputStream&);
    void serialize(OutputStream&) const final;

    CharacteristicType type() const final { return CharacteristicType::DistanceToPlayer; }
    bool check(const GW::AgentLiving& agent) const final;
    void drawSettings() final;

private:
    ComparisonOperator comp = ComparisonOperator::LessOrEqual;
    float distance = 1248.f;
};

class DistanceToTargetCharacteristic : public Characteristic {
public:
    DistanceToTargetCharacteristic() = default;
    DistanceToTargetCharacteristic(InputStream&);
    void serialize(OutputStream&) const final;

    CharacteristicType type() const final { return CharacteristicType::DistanceToTarget; }
    bool check(const GW::AgentLiving& agent) const final;
    void drawSettings() final;

private:
    ComparisonOperator comp = ComparisonOperator::LessOrEqual;
    float distance = 1248.f;
};

class ClassCharacteristic : public Characteristic {
public:
    ClassCharacteristic() = default;
    ClassCharacteristic(InputStream&);
    void serialize(OutputStream&) const final;

    CharacteristicType type() const final { return CharacteristicType::Class; }
    bool check(const GW::AgentLiving& agent) const final;
    void drawSettings() final;

private:
    Class primary = Class::Any;
    Class secondary = Class::Any;
    IsIsNot comp = IsIsNot::Is;
};

class NameCharacteristic : public Characteristic {
public:
    NameCharacteristic() = default;
    NameCharacteristic(InputStream&);
    void serialize(OutputStream&) const final;

    CharacteristicType type() const final { return CharacteristicType::Name; }
    bool check(const GW::AgentLiving& agent) const final;
    void drawSettings() final;

private:
    std::string name = "";
    IsIsNot comp = IsIsNot::Is;
};

class HPCharacteristic : public Characteristic {
public:
    HPCharacteristic() = default;
    HPCharacteristic(InputStream&);
    void serialize(OutputStream&) const final;

    CharacteristicType type() const final { return CharacteristicType::HP; }
    bool check(const GW::AgentLiving& agent) const final;
    void drawSettings() final;

private:
    float hp = 50.f;
    ComparisonOperator comp = ComparisonOperator::LessOrEqual;
};

class HPRegenCharacteristic : public Characteristic {
public:
    HPRegenCharacteristic() = default;
    HPRegenCharacteristic(InputStream&);
    void serialize(OutputStream&) const final;

    CharacteristicType type() const final { return CharacteristicType::HPRegen; }
    bool check(const GW::AgentLiving& agent) const final;
    void drawSettings() final;

private:
    int hpRegen = 0;
    ComparisonOperator comp = ComparisonOperator::Greater;
};

class SpeedCharacteristic : public Characteristic {
public:
    SpeedCharacteristic() = default;
    SpeedCharacteristic(InputStream&);
    void serialize(OutputStream&) const final;

    CharacteristicType type() const final { return CharacteristicType::Speed; }
    bool check(const GW::AgentLiving& agent) const final;
    void drawSettings() final;

private:
    float speed = 0.f;
    ComparisonOperator comp = ComparisonOperator::GreaterOrEqual;
};

class WeaponTypeCharacteristic : public Characteristic {
public:
    WeaponTypeCharacteristic() = default;
    WeaponTypeCharacteristic(InputStream&);
    void serialize(OutputStream&) const final;

    CharacteristicType type() const final { return CharacteristicType::WeaponType; }
    bool check(const GW::AgentLiving& agent) const final;
    void drawSettings() final;

private:
    WeaponType weapon = WeaponType::Any;
    IsIsNot comp = IsIsNot::Is;
};

class ModelCharacteristic : public Characteristic {
public:
    ModelCharacteristic() = default;
    ModelCharacteristic(InputStream&);
    void serialize(OutputStream&) const final;

    CharacteristicType type() const final { return CharacteristicType::Model; }
    bool check(const GW::AgentLiving& agent) const final;
    void drawSettings() final;

private:
    uint16_t modelId = 0;
    IsIsNot comp = IsIsNot::Is;
};

class AllegianceCharacteristic : public Characteristic {
public:
    AllegianceCharacteristic() = default;
    AllegianceCharacteristic(InputStream&);
    void serialize(OutputStream&) const final;

    CharacteristicType type() const final { return CharacteristicType::Allegiance; }
    bool check(const GW::AgentLiving& agent) const final;
    void drawSettings() final;

private:
    AgentType agentType = AgentType::Hostile;
    IsIsNot comp = IsIsNot::Is;
};

class StatusCharacteristic : public Characteristic {
public:
    StatusCharacteristic() = default;
    StatusCharacteristic(InputStream&);
    void serialize(OutputStream&) const final;

    CharacteristicType type() const final { return CharacteristicType::Status; }
    bool check(const GW::AgentLiving& agent) const final;
    void drawSettings() final;

private:
    Status status = Status::Alive;
    IsIsNot comp = IsIsNot::Is;
};

class SkillCharacteristic : public Characteristic {
public:
    SkillCharacteristic() = default;
    SkillCharacteristic(InputStream&);
    void serialize(OutputStream&) const final;

    CharacteristicType type() const final { return CharacteristicType::Status; }
    bool check(const GW::AgentLiving& agent) const final;
    void drawSettings() final;

private:
    GW::Constants::SkillID skill = GW::Constants::SkillID::No_Skill;
    IsIsNot comp = IsIsNot::Is;
};

class AngleToPlayerForwardCharacteristic : public Characteristic {
public:
    AngleToPlayerForwardCharacteristic() = default;
    AngleToPlayerForwardCharacteristic(InputStream&);
    void serialize(OutputStream&) const final;

    CharacteristicType type() const final { return CharacteristicType::AngleToPlayerForward; }
    bool check(const GW::AgentLiving& agent) const final;
    void drawSettings() final;

private:
    float angle = 180.;
    ComparisonOperator comp = ComparisonOperator::LessOrEqual;
};

class AngleToCameraForwardCharacteristic : public Characteristic {
public:
    AngleToCameraForwardCharacteristic() = default;
    AngleToCameraForwardCharacteristic(InputStream&);
    void serialize(OutputStream&) const final;

    CharacteristicType type() const final { return CharacteristicType::AngleToCameraForward; }
    bool check(const GW::AgentLiving& agent) const final;
    void drawSettings() final;

private:
    float angle = 180.;
    ComparisonOperator comp = ComparisonOperator::LessOrEqual;
};
