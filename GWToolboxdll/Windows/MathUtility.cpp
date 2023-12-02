#pragma once
#include "MathUtility.h"

#include <vector>
#include <algorithm>
#include <string>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Context/MapContext.h>

namespace MathUtil {
    //Interception on a straight line segment. This function assumes that there are no obstacles for any agent.
    GW::Vec2f intercept(const GW::Vec2f &chaser, const float &chaser_vel, 
        const GW::Vec2f &target, const GW::Vec2f &direction, const float &target_vel, float radius) {
        //aliases:
        auto &vc = chaser_vel;
        auto v =  direction * target_vel;
        auto &vt = target_vel;
        auto x = target - chaser;
        auto &r = radius;

        if (vc == 0.0f) [[unlikely]] {
            return chaser;
        }

        float a = vc * vc - vt * vt; //vt*vt == v*v
        float b = -2.0f * (Dot(x, v) + vt * r);
        float c = -Dot(x, x) - 2.0f * r * Dot(x, direction) - r * r; // (r * r * Dot(v, v) / (vt * vt));

        float t;
        if (fabsf(a) < 0.001f) {
            if (fabsf(b) < 0.0001f) {
                return target + r * direction;
            }
            t = -c / b;
            t = (std::max)(t, 0.0f);
        }
        else {
            float s = b * b - 4.0f * a * c;
            if (s < 0.0f) {
                s = 0.0f; // return target + r * direction;
            }
            s = sqrtf(s);
            a =  1.0f / (2.0f * a);

            float t1 = (-b + s) * a;
            float t2 = (-b - s) * a;
            if (t1 > 0.0f && t2 > 0.0f) {
                t = (std::min)(t1, t2);
            }
            else {
                t = (std::max)(t1, t2);
                t = (std::max)(t, 0.0f);
            }  
        }

        GW::Vec2f p = x + v * t + r * direction;
        return chaser + p;
    }

    GW::Vec2f GetVec2f(const GW::Vec3f &r) {
        return { r.x, r.y };
    }

    GW::Vec3f GetVec3f(const GW::GamePos& gp) {
        auto map = GW::GetMapContext();
        if (!map || map->sub1->sub2->pmaps.size() <= gp.zplane) [[unlikely]] {
            auto player = GW::Agents::GetPlayer();
            if (player) return GW::Vec3f(gp.x, gp.y, player->z);
            else return GW::Vec3f(gp.x, gp.y, 0.0f);
        }

        GW::Vec3f pos{ gp.x, gp.y, 0.0f };
        GW::Map::QueryAltitude(gp, 5.0f, pos.z);
        return pos;
    }

    float sign(const float& a) {
        return (float)((a > 0.0f) - (a < 0.0f));
    }

    GW::Vec2f sign(const GW::Vec2f& a) {
        return { sign(a.x), sign(a.y) };
    }

    GW::Vec2f Hadamard(const GW::Vec2f& lhs, const GW::Vec2f& rhs) {
        return { lhs.x * rhs.x, lhs.y * rhs.y };
    }

    float Cross(const GW::Vec2f& lhs, const GW::Vec2f& rhs) {
        return (lhs.x * rhs.y) - (lhs.y * rhs.x);
    }

    float Dot(GW::Vec2f lhs, GW::Vec2f rhs) {
        return (lhs.x * rhs.x) + (lhs.y * rhs.y);
    }

    // Given three collinear points p, q, r, the function checks if
    // point q lies on line segment 'pr'
    bool onSegment(const GW::Vec2f& p, const GW::Vec2f& q, const GW::Vec2f& r)
    {
        if (q.x <= (std::max)(p.x, r.x) && q.x >= (std::min)(p.x, r.x) &&
            q.y <= (std::max)(p.y, r.y) && q.y >= (std::min)(p.y, r.y))
            return true;

        return false;
    }

    // The main function that returns true if line segment 'p1q1'
    // and 'p2q2' intersect.
    bool Intersect(const GW::Vec2f& p1, const GW::Vec2f& q1, const GW::Vec2f& p2, const GW::Vec2f& q2) {
        float eps = 0.001f;
        float denom = (q2.y - p2.y) * (q1.x - p1.x) - (q2.x - p2.x) * (q1.y - p1.y);

        /* Are the line parallel */
        if (fabsf(denom) < eps) {
            return false;
        }

        float numera = (q2.x - p2.x) * (p1.y - p2.y) - (q2.y - p2.y) * (p1.x - p2.x);
        float numerb = (q1.x - p1.x) * (p1.y - p2.y) - (q1.y - p1.y) * (p1.x - p2.x);

        /* Are the line coincident? */
        if (fabsf(numera) < eps && fabsf(numerb) < eps) {
            return true;
        }

        /* Is the intersection along the the segments */
        float mua = numera / denom;
        float mub = numerb / denom;
        if (mua < 0.0f || mua > 1.0f || mub < 0.0f || mub > 1.0f) {
            return false;
        }
        return true;
    }

    bool between(float x, float min, float max) {
        if (min <= max)
            return min <= x && x <= max;
        else
            return min >= x && x >= max;
    }

    bool collinear(const GW::Vec2f& a1, const GW::Vec2f& a2, const GW::Vec2f& b1, const GW::Vec2f& b2) {
        float tolerance = 0.1f;

        auto s = b2 - b1;
        auto inv = 1.0f / Dot(s, s) * s;
        auto q1 = b1 + Dot(a1 - b1, s) * inv;
        auto q2 = b1 + Dot(a2 - b1, s) * inv;

        if (GetSquaredNorm(a1 - q1) > tolerance || GetSquaredNorm(a2 - q2) > tolerance)
            return false;

        return onSegment(a1, b1, a2) || onSegment(a1, b2, a2) || onSegment(b1, a1, b2) || onSegment(b1, a2, b2);
    }
}
