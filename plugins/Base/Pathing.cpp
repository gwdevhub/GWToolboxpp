#include <Pathing.h>

#include <array>
#include <cmath>
#include <Windows.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Utilities/Scanner.h>

typedef void(__cdecl* FindPath_pt)(PathPoint* start, PathPoint* goal, float range, uint32_t maxCount, uint32_t* count, PathPoint* pathArray);
static FindPath_pt FindPath_Func = nullptr;

constexpr uint32_t pathingCount = 10;
constexpr float pathingRange = 10'000.f;
constexpr auto sampleCount = 16;

float Cross(const GW::Vec2f& lhs, const GW::Vec2f& rhs)
{
    return (lhs.x * rhs.y) - (lhs.y * rhs.x);
}

void InitializePathing() 
{
    FindPath_Func = (FindPath_pt)GW::Scanner::Find("\x8b\x5d\x0c\x8b\x7d\x08\x8b\x70\x14\xd9", "xxxxxxxxxx", -0xE);
}

enum class PathingResult { CanPath, CanPartiallyPath, CannotPath, Unknown };

PathingResult canPathToTarget(const PathPoint& playerPathPoint, const PathPoint& targetPos)
{
    if (!FindPath_Func) 
    {
        return PathingResult::Unknown;
    }

    auto start = playerPathPoint;
    auto end = targetPos;

    std::array<PathPoint, pathingCount> pathArray;
    uint32_t count = pathingCount;
    FindPath_Func(&start, &end, pathingRange, count, &count, &pathArray[0]);

    if (count == 1 && GW::GetSquareDistance(pathArray[0].pos, end.pos) > 10) 
        return PathingResult::CannotPath;
    if (GW::GetSquareDistance(start.pos, pathArray[0].pos) > GW::Constants::SqrRange::Compass)
        return PathingResult::CanPath;
    if (GW::GetSquareDistance(start.pos, end.pos) > GW::Constants::SqrRange::Compass) 
        return PathingResult::CanPartiallyPath;
    return PathingResult::CanPath;
}

OutcomeChances toChances(PathingResult res)
{
    OutcomeChances c;
    switch (res) {
        case PathingResult::CanPath:
            c.success = 1.f;
            break;
        case PathingResult::CanPartiallyPath:
            c.partial = 1.f;
            break;
        case PathingResult::CannotPath:
        case PathingResult::Unknown:
            c.failure = 1.f;
            break;
    }
    return c;
}

bool pointOnTrapezoid(const GW::GamePos& p, const GW::PathingTrapezoid* trap)
{
    if (!trap) return false;

    //  a----d
    //   \    \
    //    b____c
    const auto a = GW::Vec2f{trap->XTL, trap->YT};
    const auto b = GW::Vec2f{trap->XBL, trap->YB};
    const auto c = GW::Vec2f{trap->XBR, trap->YB};
    const auto d = GW::Vec2f{trap->XTR, trap->YT};

    // See GWCA pathing.cpp:IsOnPathingTrapezoid
    constexpr float tolerance = 2.0f;
    if (a.y < p.y || b.y > p.y) return false;
    if (b.x > p.x && a.x > p.x) return false;
    if (c.x < p.x && d.x < p.x) return false;
    const auto ab = b - a, cd = d - c, pa = a - p, pc = c - p;
    if (Cross(ab, pa) > tolerance) return false;
    if (Cross(cd, pc) > tolerance) return false;
    return true;
}

const GW::PathingTrapezoid* findContainingNeighbour(const GW::GamePos& pos, const GW::PathingTrapezoid* trap)
{
    if (!trap) return nullptr;

    if (pointOnTrapezoid(pos, trap)) return trap;
    if (pointOnTrapezoid(pos, trap->adjacent[0])) return trap->adjacent[0];
    if (pointOnTrapezoid(pos, trap->adjacent[1])) return trap->adjacent[1];
    if (pointOnTrapezoid(pos, trap->adjacent[2])) return trap->adjacent[2];
    if (pointOnTrapezoid(pos, trap->adjacent[3])) return trap->adjacent[3];

    return nullptr;
}

const GW::PathingTrapezoid* findTrapezoid(const GW::GamePos& pos, const GW::PathingMapArray* path_map)
{
    for (const auto& map : *path_map) {
        for (uint32_t i = 0u; i < map.trapezoid_count; ++i) {
            if (pointOnTrapezoid(pos, &map.trapezoids[i])) {
                return &map.trapezoids[i];
            }
        }
    }
    return nullptr;
}

/// @param direction: int 0..15, picks direction to offset to
GW::GamePos getOffsetPosition(GW::GamePos center, float distance, uint8_t direction)
{
    constexpr auto pi = 3.14159265359f;
    const auto phi = direction * pi / sampleCount;
    center.y += distance * std::sin(phi);
    center.x += distance * std::cos(phi);
    return center;
}

OutcomeChances dcPrediction(const PathPoint& playerPathPoint, const GW::Agent* target, const GW::PathingMapArray* path_map)
{
    if (!target) return {};

    constexpr float dcOffset = 100.f;

    OutcomeChances chances;
    const auto handleResult = [&](PathingResult res) {
        switch (res) 
        {
            case PathingResult::CanPath:
                chances.success += 1. / sampleCount;
                break;
            case PathingResult::CanPartiallyPath:
                chances.partial += 1. / sampleCount;
                break;
            case PathingResult::CannotPath:
                chances.failure += 1. / sampleCount;
                break;
            case PathingResult::Unknown:
                break;
        }
    };

    const auto targetTrap = findTrapezoid(target->pos, path_map);
    if (!targetTrap) return {};

    for (uint8_t direction = 0u; direction < sampleCount; ++direction) 
    {
        GW::Vec2f pos{};
        const GW::PathingTrapezoid* trap = nullptr;

        auto currentOffset = dcOffset;
        while (!trap) 
        {
            pos = getOffsetPosition(target->pos, currentOffset, direction);
            trap = findContainingNeighbour(pos, targetTrap);
            currentOffset /= 2;
        }
        handleResult(canPathToTarget(playerPathPoint, {GW::GamePos{pos.x, pos.y, target->pos.zplane}, trap}));
    }

    return chances;
}

OutcomeChances sohPrediction(const PathPoint& playerPathPoint, const PathPoint& sohSpot) 
{
    return toChances(canPathToTarget(playerPathPoint, sohSpot));
}
