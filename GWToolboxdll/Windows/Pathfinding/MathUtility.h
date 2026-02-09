#pragma once
#include <GWCA/GameContainers/GamePos.h>

namespace MathUtil {
    GW::Vec3f GetVec3f(const GW::GamePos &gp);

    float sign(const float &a);

    GW::Vec2f sign(const GW::Vec2f &a);

    GW::Vec2f Hadamard(const GW::Vec2f &lhs, const GW::Vec2f &rhs);

    float Cross(const GW::Vec2f &lhs, const GW::Vec2f &rhs);

    float Dot(const GW::Vec2f& lhs, const GW::Vec2f& rhs);

    // Given three collinear points p, q, r, the function checks if
    // point q lies on line segment 'pr'
    bool onSegment(const GW::Vec2f &p, const GW::Vec2f &q, const GW::Vec2f &r);

    // The main function that returns true if line segment 'p1q1'
    // and 'p2q2' intersect.
    bool Intersect(const GW::Vec2f &p1, const GW::Vec2f &q1, const GW::Vec2f &p2, const GW::Vec2f &q2);

    bool between(float x, float min, float max);

    bool collinear(const GW::Vec2f &a1, const GW::Vec2f &a2, const GW::Vec2f &b1, const GW::Vec2f &b2);

    float getDistanceFromLine(const GW::Vec2f& a1, const GW::Vec2f& a2, const GW::Vec2f& p);
}
