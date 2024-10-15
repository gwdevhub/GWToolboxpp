#include "stdafx.h"

#include "MissionMapWidget.h"

#include <GWCA/Context/MapContext.h>
#include <GWCA/Context/GameplayContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Widgets/Minimap/Minimap.h>
#include <Utils/ToolboxUtils.h>

namespace {

}

void MissionMapWidget::Draw(IDirect3DDevice9*)
{
    const auto mission_map_context = GW::Map::GetMissionMapContext();
    const auto mission_map_frame = mission_map_context ? GW::UI::GetFrameById(mission_map_context->frame_id) : nullptr;
    if (!(mission_map_frame && mission_map_frame->IsVisible()))
        return;

    const auto root = GW::UI::GetRootFrame();
    const auto mission_map_top_left = mission_map_frame->position.GetContentTopLeft(root);
    const auto mission_map_bottom_right = mission_map_frame->position.GetContentBottomRight(root);

    const auto viewport = ImGui::GetMainViewport();
    const auto& viewport_offset = viewport->Pos;
    const auto draw_list = ImGui::GetBackgroundDrawList(viewport);

    const auto& lines = Minimap::Instance().custom_renderer.GetLines();

    // TODO: Look at the minimap widget and world map widget; use translations and other bollocks to draw lines into the mission map.

    const auto gameplay_context = GW::GetGameplayContext();
    const auto& mission_map_zoom = gameplay_context->mission_map_zoom; // e.g. scroll wheel in/out
    const auto& mission_map_center_pos = mission_map_context->player_mission_map_pos; // e.g. player position in the world (this could change depending on observing etc)
    const auto& mission_map_last_click_location = mission_map_context->last_mouse_location; // e.g. when you drag on the mission map
    const auto& current_pan_offset = mission_map_context->h003c->mission_map_pan_offset;

    (viewport_offset, lines, draw_list, mission_map_zoom, mission_map_center_pos, mission_map_last_click_location);

    // Might need to rip off GamePosToWorldMap etc from world map widget

    // Bounds for mission map:
    draw_list->AddRect({ mission_map_top_left.x, mission_map_top_left.y }, { mission_map_bottom_right.x, mission_map_bottom_right.y }, IM_COL32_WHITE);
    float y = mission_map_top_left.y;
    draw_list->AddText({ mission_map_top_left.x, y }, IM_COL32_WHITE, "TODO: Look at the minimap widget and world map widget.\nUse translations and other bollocks to draw lines into the mission map.");
    y += 48.f;
    draw_list->AddText({ mission_map_top_left.x, y }, IM_COL32_WHITE, std::format("Mission Map rect: {}, {}, {}, {}\nZoom level: {}\nCenter pos (player):{}, {}\nPan:{}, {}", 
        mission_map_top_left.x, mission_map_top_left.y, mission_map_bottom_right.x, mission_map_bottom_right.y, 
        mission_map_zoom, 
        mission_map_center_pos.x, mission_map_center_pos.y,
        current_pan_offset.x, current_pan_offset.y).c_str());

    // RENDER ME!!
}
