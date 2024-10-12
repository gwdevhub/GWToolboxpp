#pragma once

#include <io.h>

namespace GW 
{
    struct AgentLiving;
}

// This struct is append only, do NOT change the ordering of the values or add new ones at any place but the end
enum class CharacteristicType : int {
    Position,
    PositionPolygon,
    DistanceToPlayer,
    DistanceToTarget,
    Class,
    Name,
    HP,
    Speed,
    HPRegen,
    WeaponType,
    Model,
    Allegiance,
    Status,
    Skill,
    Bond,
    AngleToPlayerForward,
    AngleToCameraForward,
    DistanceToModelId,

    Count
};

class Characteristic {
public:
    virtual ~Characteristic() {}
    virtual CharacteristicType type() const = 0;
    virtual bool check(const GW::AgentLiving& agent) const = 0;
    virtual void drawSettings() = 0;
    virtual void serialize(OutputStream& stream) const { stream << "X" << type(); }
};
using CharacteristicPtr = std::unique_ptr<Characteristic>;
