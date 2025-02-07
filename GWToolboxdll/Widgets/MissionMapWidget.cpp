#include "stdafx.h"

#include <GWCA/Context/GameplayContext.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Utilities/Hooker.h>

#include <Widgets/MissionMapWidget.h>
#include <Widgets/WorldMapWidget.h>
#include <Widgets/Minimap/Minimap.h>
#include <Modules/QuestModule.h>
#include <ImGuiAddons.h>

namespace {
    bool draw_all_terrain_lines = false;
    bool draw_all_minimap_lines = true;

    GW::Vec2f mission_map_top_left;
    GW::Vec2f mission_map_bottom_right;
    GW::Vec2f mission_map_scale;
    float mission_map_zoom;

    GW::Vec2f mission_map_center_pos;
    GW::Vec2f mission_map_last_click_location;
    GW::Vec2f current_pan_offset;

    GW::Vec2f mission_map_screen_pos;
    GW::Vec2f world_map_click_pos;

    bool right_clicking = false;

    GW::UI::Frame* mission_map_frame = nullptr;

    bool IsScreenPosOnMissionMap(const GW::Vec2f& screen_pos)
    {
        if (!(mission_map_frame && mission_map_frame->IsVisible()))
            return false;
        return (screen_pos.x >= mission_map_top_left.x && screen_pos.x <= mission_map_bottom_right.x &&
                screen_pos.y >= mission_map_top_left.y && screen_pos.y <= mission_map_bottom_right.y);
    }

    GW::Vec2f GetMissionMapScreenCenterPos()
    {
        return mission_map_top_left + (mission_map_bottom_right - mission_map_top_left) / 2;
    }

    // Given the world pos, calculate where the mission map would draw this on-screen
    bool WorldMapCoordsToMissionMapScreenPos(const GW::Vec2f& world_map_position, GW::Vec2f& screen_coords)
    {
        const auto offset = (world_map_position - current_pan_offset);
        const auto scaled_offset = GW::Vec2f(offset.x * mission_map_scale.x, offset.y * mission_map_scale.y);
        screen_coords = (scaled_offset * mission_map_zoom + mission_map_screen_pos);
        return true;
    }

    // Given the on-screen pos, calculate where the mission map coords land
    GW::Vec2f ScreenPosToMissionMapCoords(const GW::Vec2f screen_position)
    {
        GW::Vec2f unscaled_offset = (screen_position - mission_map_screen_pos) / mission_map_zoom;
        GW::Vec2f offset(
            unscaled_offset.x / mission_map_scale.x,
            unscaled_offset.y / mission_map_scale.y
        );
        return offset + current_pan_offset;
    }

    //
    bool GamePosToMissionMapScreenPos(const GW::GamePos& game_map_position, GW::Vec2f& screen_coords)
    {
        GW::Vec2f world_map_pos;
        return WorldMapWidget::GamePosToWorldMap(game_map_position, world_map_pos) && WorldMapCoordsToMissionMapScreenPos(world_map_pos, screen_coords);
    }

    std::vector<GW::UI::UIMessage> messages_hit;
    GW::UI::UIInteractionCallback OnMissionMap_UICallback_Func = 0, OnMissionMap_UICallback_Ret = 0;

    void OnMissionMap_UICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam)
    {
        GW::Hook::EnterHook();
        switch (message->message_id) {
            case GW::UI::UIMessage::kInitFrame:
                OnMissionMap_UICallback_Ret(message, wparam, lparam);
                mission_map_frame = GW::UI::GetFrameById(message->frame_id);
                break;
            case GW::UI::UIMessage::kDestroyFrame:
                mission_map_frame = nullptr;
                OnMissionMap_UICallback_Ret(message, wparam, lparam);
                break;
            default:
                OnMissionMap_UICallback_Ret(message, wparam, lparam);
                break;
        }
        GW::Hook::LeaveHook();
    }



    bool HookMissionMapFrame()
    {
        if (OnMissionMap_UICallback_Func)
            return true;
        const auto mission_map_context = GW::Map::GetMissionMapContext();
        mission_map_frame = mission_map_context ? GW::UI::GetFrameById(mission_map_context->frame_id) : nullptr;
        if (!(mission_map_frame && mission_map_frame->frame_callbacks[0]))
            return false;
        OnMissionMap_UICallback_Func = mission_map_frame->frame_callbacks[0];
        GW::Hook::CreateHook((void**)&OnMissionMap_UICallback_Func, OnMissionMap_UICallback, (void**)&OnMissionMap_UICallback_Ret);
        GW::Hook::EnableHooks(OnMissionMap_UICallback_Func);
        return true;
    }

    bool InitializeMissionMapParameters()
    {
        const auto gameplay_context = GW::GetGameplayContext();
        const auto mission_map_context = GW::Map::GetMissionMapContext();
        if (!(mission_map_frame && mission_map_frame->IsVisible()))
            return false;

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

    bool MissionMapContextMenu(void*)
    {
        if (!(mission_map_frame && mission_map_frame->IsVisible()))
            return false;
        const auto c = ImGui::GetCurrentContext();
        auto viewport_offset = c->CurrentViewport->Pos;
        viewport_offset.x *= -1;
        viewport_offset.y *= -1;

        ImGui::Text("%.2f, %.2f", world_map_click_pos.x, world_map_click_pos.y);
#ifdef _DEBUG
        GW::GamePos game_pos;
        if (WorldMapWidget::WorldMapToGamePos(world_map_click_pos, game_pos)) {
            ImGui::Text("%.2f, %.2f", game_pos.x, game_pos.y);
        }
#endif
        if (ImGui::Button("Place Marker")) {
            GW::GameThread::Enqueue([] {
                QuestModule::SetCustomQuestMarker(world_map_click_pos, true);
            });
            return false;
        }
        if (QuestModule::GetCustomQuestMarker()) {
            if (ImGui::Button("Remove Marker")) {
                GW::GameThread::Enqueue([] {
                    QuestModule::SetCustomQuestMarker({0, 0});
                });
                return false;
            }
        }
        return true;
    }
}

void MissionMapWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    LOAD_BOOL(draw_all_terrain_lines);
    LOAD_BOOL(draw_all_minimap_lines);
}

void MissionMapWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_BOOL(draw_all_terrain_lines);
    SAVE_BOOL(draw_all_minimap_lines);
}

void MissionMapWidget::Draw(IDirect3DDevice9*)
{
    if (!visible)
        return;
    if (!HookMissionMapFrame())
        return;
    if (!InitializeMissionMapParameters())
        return;

    const auto viewport = ImGui::GetMainViewport();
    const auto draw_list = ImGui::GetBackgroundDrawList(viewport);
    const auto& lines = Minimap::Instance().custom_renderer.GetLines();

    draw_list->PushClipRect({mission_map_top_left.x, mission_map_top_left.y}, {mission_map_bottom_right.x, mission_map_bottom_right.y});

    const auto map_id = GW::Map::GetMapID();
    for (const auto& line : lines) {
        if (!line->visible) continue;
        if (!line->draw_on_mission_map &&
            !(draw_all_minimap_lines && line->draw_on_minimap) &&
            !(draw_all_terrain_lines && line->draw_on_terrain))
            continue;
        if (line->map != map_id) continue;

        GW::Vec2f projected_p1;
        if (!GamePosToMissionMapScreenPos(line->p1, projected_p1))
            continue;
        GW::Vec2f projected_p2;
        if (!GamePosToMissionMapScreenPos(line->p2, projected_p2))
            continue;
        draw_list->AddLine(projected_p1, projected_p2, line->color, 1.0F);
    }

    draw_list->PopClipRect();
}

void MissionMapWidget::DrawSettingsInternal()
{
    ImGui::Checkbox("Draw all terrain lines", &draw_all_terrain_lines);
    ImGui::Checkbox("Draw all minimap lines", &draw_all_minimap_lines);
}

bool MissionMapWidget::WndProc(const UINT Message, WPARAM, LPARAM lParam)
{
    switch (Message) {
        case WM_GW_RBUTTONCLICK:
            GW::Vec2f cursor_pos = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (!IsScreenPosOnMissionMap(cursor_pos))
                break;
            if (GW::UI::GetCurrentTooltip())
                break;
            world_map_click_pos = ScreenPosToMissionMapCoords(cursor_pos);
            ImGui::SetContextMenu(MissionMapContextMenu);
            break;
    }
    return false;
}

void MissionMapWidget::Terminate()
{
    ToolboxWidget::Terminate();
    if (OnMissionMap_UICallback_Func)
        GW::Hook::RemoveHook(OnMissionMap_UICallback_Func);
    OnMissionMap_UICallback_Func = nullptr;
}
