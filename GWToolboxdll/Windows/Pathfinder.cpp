#include "stdafx.h"

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Pathing.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <Utils/GuiUtils.h>
#include <GWToolbox.h>
#include <Logger.h>

#include <Timer.h>

#include <Windows/Pathfinder.h>


namespace {

    GW::Vec2f target_location = { 0,0 };
    std::vector<const GW::PathingTrapezoid*> path_chosen;
    bool recalculate_path = false;
    size_t draw_pos = 0;
    clock_t last_draw = 0;

    void CmdPathfind(const wchar_t*, const int, const LPWSTR*) {
        const auto target = GW::Agents::GetTarget();
        if (!target) {
            Log::Error("No target");
            return;
        }
        target_location = target->pos;
        recalculate_path = true;
    }

    const GW::Vec2f* GetCurrentPos() {
        const auto player = GW::Agents::GetPlayer();
        if (!player) return nullptr;
        // No copy thanks.
        return (GW::Vec2f*) & player->pos;
    }
    constexpr float GetSquareDistance(const GW::Vec2f& p1, const GW::Vec2f& p2) {
        return (p1.x - p2.x) * (p1.x - p2.x) +
            (p1.y - p2.y) * (p1.y - p2.y);
    }

    // Calculate closest vertex to trapezoid. if less than min_distance, will return value.
    constexpr float GetClosestVertex(const GW::Vec2f& point, GW::PathingTrapezoid* trapezoid, float min_distance, GW::Vec2f* closest_vertex) {
        if (!trapezoid)
            return .0f;
        GW::Vec2f top_left = { trapezoid->XTL, trapezoid->YT };
        GW::Vec2f top_right = { trapezoid->XTR, trapezoid->YT };
        GW::Vec2f bottom_right = { trapezoid->XBR, trapezoid->YB };
        GW::Vec2f bottom_left = { trapezoid->XBL, trapezoid->YB };

        GW::Vec2f* closest = nullptr;

        float dist = ::GetSquareDistance(point, top_left);
        if (dist < min_distance) {
            min_distance = dist;
            closest = &top_left;
        }
        dist = ::GetSquareDistance(point, top_right);
        if (dist < min_distance) {
            min_distance = dist;
            closest = &top_right;
        }
        dist = ::GetSquareDistance(point, bottom_right);
        if (dist < min_distance) {
            min_distance = dist;
            closest = &bottom_right;
        }
        dist = ::GetSquareDistance(point, bottom_left);
        if (dist < min_distance) {
            min_distance = dist;
            closest = &bottom_left;
        }

        if (closest) {
            *closest_vertex = *closest;
            return min_distance;
        }
        return .0f;
    }
    const GW::PathingTrapezoid* GetClosestTrapezoid(const GW::Vec2f& point, GW::Vec2f* closest_vertex) {
        const auto pathing_maps = GW::Map::GetPathingMap();

        float closest_distance = 99999.0f;
        float this_distance = .0f;
        GW::PathingTrapezoid* closest_trapezoid = nullptr;
        if (!pathing_maps) return nullptr;
        for (auto& pathing_map : *pathing_maps) {
            for (uint32_t i = 0; i < pathing_map.trapezoid_count; i++) {
                this_distance = ::GetClosestVertex(point, &pathing_map.trapezoids[i], closest_distance, closest_vertex);
                if (this_distance != .0f && (!closest_trapezoid || this_distance < closest_distance)) {
                    closest_trapezoid = &pathing_map.trapezoids[i];
                    closest_distance = this_distance;
                }
            }
        }
        return closest_trapezoid;
    }
    const GW::PathingTrapezoid* GetClosestNeighbor(const GW::Vec2f& point, const GW::PathingTrapezoid* trapezoid, GW::Vec2f* closest_vertex) {
        float closest_distance = 99999.0f;
        float this_distance = .0f;
        GW::PathingTrapezoid* closest_trapezoid = nullptr;

        for (uint32_t i = 0; i < _countof(trapezoid->adjacent); i++) {
            this_distance = ::GetClosestVertex(point, trapezoid->adjacent[i], closest_distance, closest_vertex);
            if (this_distance != .0f && (!closest_trapezoid || this_distance < closest_distance)) {
                closest_trapezoid = trapezoid->adjacent[i];
            }
        }
        return closest_trapezoid;
    }

    bool IsPointWithinTrapezoid(const GW::Vec2f& point, const GW::PathingTrapezoid* trapezoid) {

        float x_min = trapezoid->XBL;
        if (trapezoid->XTL < x_min)
            x_min = trapezoid->XTL;
        float x_max = trapezoid->XBR;
        if (trapezoid->XTR > x_max)
            x_max = trapezoid->XTR;


        if (point.x < x_min || point.x > x_max || point.y < trapezoid->YT || point.y > trapezoid->YB) {
            // Definitely not within the polygon!
            return false;
        }
        // TODO: Maths.
        return true;
    }

    void Recalculate() {
        recalculate_path = false;
        Log::Info("Recalculating path");
        GW::Vec2f closest_vertex;
        const auto pos = GetCurrentPos();
        if (!pos) {
            Log::Error("No player pos?");
            return;
        }
        path_chosen.clear();
        auto trapezoid = GetClosestTrapezoid(*pos, &closest_vertex);
        while (trapezoid) {
            if (std::find(path_chosen.begin(),path_chosen.end(), trapezoid) != path_chosen.end())
                break;
            path_chosen.push_back(trapezoid);
            if (IsPointWithinTrapezoid(target_location, trapezoid))
                break;

            trapezoid = GetClosestNeighbor(target_location, trapezoid, &closest_vertex);
        }
        if (!path_chosen.size()) {
            Log::Error("No closest trapezoid?");
            return;
        }
        Log::Info("%d trapezoids, check compass", path_chosen.size());
        draw_pos = 0;
    }
}

void Pathfinder::Draw(IDirect3DDevice9*)
{
    if (recalculate_path) {
        Recalculate();
    }
    if (draw_pos < path_chosen.size()
        && TIMER_DIFF(last_draw) > 1000) {
        const auto trapezoid = path_chosen[draw_pos];
        GW::Vec2f points[4];
        points[0] = { trapezoid->XTL, trapezoid->YT };
        points[1] = { trapezoid->XTR, trapezoid->YT };
        points[2] = { trapezoid->XBR, trapezoid->YB };
        points[3] = { trapezoid->XBL, trapezoid->YB };
        GW::UI::DrawOnCompass((unsigned int)&trapezoid, _countof(points), points);

        GW::Packet::StoC::CompassEvent event;
        event.NumberPts = _countof(points);
        event.Player = GW::Agents::GetPlayerAsAgentLiving()->player_number;
        for (size_t i = 0; i < event.NumberPts; i++) {
            event.points[i].x = (short)points[i].x;
            event.points[i].y = (short)points[i].y;
        }
        GW::StoC::EmulatePacket(&event);
        last_draw = TIMER_INIT();
        draw_pos++;
    }

}

void Pathfinder::Initialize()
{
    ToolboxWindow::Initialize();

    GW::Chat::CreateCommand(L"pathfind", CmdPathfind);
}
