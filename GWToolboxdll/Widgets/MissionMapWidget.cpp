#include "stdafx.h"

#include "MissionMapWidget.h"

#include <GWCA/Context/MapContext.h>
#include <GWCA/Context/GameplayContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Widgets/Minimap/Minimap.h>
#include <Utils/ToolboxUtils.h>
#include "WorldMapWidget.h"

namespace {
    GW::Vec2f mission_map_top_left;
    GW::Vec2f mission_map_bottom_right;
    GW::Vec2f mission_map_scale;
    float mission_map_zoom;

    GW::Vec2f mission_map_center_pos;
    GW::Vec2f mission_map_last_click_location;
    GW::Vec2f current_pan_offset;

    GW::Vec2f mission_map_screen_pos;

    GW::Vec2f GetMissionMapScreenCenterPos() {
        return mission_map_top_left + (mission_map_bottom_right - mission_map_top_left) / 2;
    }

    GW::Vec2f ProjectWorldMapToScreen(const GW::Vec2f position) {
        const auto offset = (position - current_pan_offset);
        const auto scaled_offset = GW::Vec2f(offset.x * mission_map_scale.x, offset.y * mission_map_scale.y);
        return (scaled_offset * mission_map_zoom + mission_map_screen_pos);
    }

    GW::Vec2f ProjectGameMapToScreen(const GW::GamePos position)
    {
        GW::Vec2f world_map_pos;
        if (!WorldMapWidget::GamePosToWorldMap(position, &world_map_pos))
        {
            return GW::Vec2f(0, 0);
        }

        return ProjectWorldMapToScreen(world_map_pos);
    }

    bool InitializeMissionMapParameters() {
        const auto gameplay_context = GW::GetGameplayContext();
        const auto mission_map_context = GW::Map::GetMissionMapContext();
        const auto mission_map_frame = mission_map_context ? GW::UI::GetFrameById(mission_map_context->frame_id) : nullptr;
        if (!(mission_map_frame && mission_map_frame->IsVisible())) return false;

        const auto root = GW::UI::GetRootFrame();
        mission_map_top_left = mission_map_frame->position.GetContentTopLeft(root);
        mission_map_bottom_right = mission_map_frame->position.GetContentBottomRight(root);
        mission_map_scale = mission_map_frame->position.GetViewportScale(root);
        mission_map_zoom = gameplay_context->mission_map_zoom;       
        mission_map_center_pos = mission_map_context->player_mission_map_pos;
        mission_map_last_click_location = mission_map_context->last_mouse_location;
        current_pan_offset = mission_map_context->h003c->mission_map_pan_offset;
        mission_map_screen_pos = GetMissionMapScreenCenterPos();
        return true;
    }
}

void MissionMapWidget::Draw(IDirect3DDevice9*)
{
    if (!InitializeMissionMapParameters()) {
        return;
    }

    const auto viewport = ImGui::GetMainViewport();
    const auto draw_list = ImGui::GetBackgroundDrawList(viewport);
    const auto& lines = Minimap::Instance().custom_renderer.GetLines();
    
    draw_list->PushClipRect({mission_map_top_left.x, mission_map_top_left.y}, {mission_map_bottom_right.x, mission_map_bottom_right.y});

    const auto map_id = GW::Map::GetMapID();
    for (const auto& line : lines) {
        if (!line->visible) continue;
        if (line->map != map_id) continue;

        const auto projected_p1 = ProjectGameMapToScreen(line->p1);
        const auto projected_p2 = ProjectGameMapToScreen(line->p2);
        draw_list->AddLine(projected_p1, projected_p2, line->color, 1.0F);
    }

    draw_list->PopClipRect();
}
