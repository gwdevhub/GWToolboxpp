#include "stdafx.h"

#include <GWCA/Constants/Maps.h>

#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Pathing.h>
#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Context/MapContext.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Widgets/WorldMapWidget.h>
#include <Widgets/Minimap/Minimap.h>
#include <Modules/GwDatTextureModule.h>
#include <Modules/Resources.h>
#include <Windows/TravelWindow.h>

#include <Utils/GuiUtils.h>
#include <Utils/ToolboxUtils.h>

#include "Defines.h"

#include <ImGuiAddons.h>
#include <Modules/QuestModule.h>
#include <GWCA/Managers/AgentMgr.h>
#include <corecrt_math_defines.h>
#include <Utils/ArenaNetFileParser.h>


namespace {

    ImRect controls_window_rect = {0, 0, 0, 0};

    IDirect3DTexture9** quest_icon_texture = nullptr;
    IDirect3DTexture9** player_icon_texture = nullptr;

    bool showing_all_outposts = false;
    bool apply_quest_colors = false;

    bool drawn = false;

    GW::MemoryPatcher view_all_outposts_patch;
    GW::MemoryPatcher view_all_carto_areas_patch;

    uint32_t __cdecl GetCartographyFlagsForArea(uint32_t, uint32_t, uint32_t, uint32_t)
    {
        return 0xffffffff;
    }

    bool world_map_clicking = false;
    GW::Vec2f world_map_click_pos;

    GW::Constants::QuestID hovered_quest_id = GW::Constants::QuestID::None;
    GuiUtils::EncString hovered_quest_name;
    GuiUtils::EncString hovered_quest_description;

    bool show_lines_on_world_map = true;
    bool showing_all_quests = true;

    bool MapContainsWorldPos(GW::Constants::MapID map_id, const GW::Vec2f& world_map_pos, GW::Constants::Campaign campaign)
    {
        const auto map = GW::Map::GetMapInfo(map_id);
        if (!(map && map->campaign == campaign))
            return false;
        ImRect map_bounds;
        return GW::Map::GetMapWorldMapBounds(map, &map_bounds) && map_bounds.Contains(world_map_pos);
    }

    bool WorldMapContextMenu(void*)
    {
        if (!GW::Map::GetWorldMapContext())
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
        const auto map_id = WorldMapWidget::GetMapIdForLocation(world_map_click_pos);
        ImGui::TextUnformatted(Resources::GetMapName(map_id)->string().c_str());

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

    bool HoveredQuestContextMenu(void* wparam)
    {
        if (!GW::Map::GetWorldMapContext())
            return false;
        const auto quest_id = static_cast<GW::Constants::QuestID>(reinterpret_cast<uint32_t>(wparam));
        const auto quest = GW::QuestMgr::GetQuest(quest_id);
        if (!quest)
            return false;
        if (!hovered_quest_name.IsDecoding())
            hovered_quest_name.reset(quest->name);
        ImGui::TextUnformatted(hovered_quest_name.string().c_str());

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImColor(0, 0, 0, 0).Value);
        const auto size = ImVec2(250.0f * ImGui::GetIO().FontGlobalScale, 0);
        ImGui::Separator();
        const bool set_active = ImGui::Button("Set active quest", size);
        const bool travel = ImGui::Button("Travel to nearest outpost", size);
        const bool wiki = ImGui::Button("Guild Wars Wiki", size);

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        if (set_active) {
            GW::GameThread::Enqueue([quest_id] {
                GW::QuestMgr::SetActiveQuestId(quest_id);
            });
            return false;
        }
        if (travel) {
            if (TravelWindow::Instance().TravelNearest(quest->map_to))
                return false;
        }
        if (wiki) {
            GW::GameThread::Enqueue([quest_id] {
                if (GW::QuestMgr::GetQuest(quest_id)) {
                    const auto wiki_url = std::format("{}Game_lmink:Quest_{}", GuiUtils::WikiUrl(L""), static_cast<uint32_t>(quest_id));
                    SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, (void*)wiki_url.c_str());
                }
            });
            return false;
        }
        return true;
    }

    uint32_t GetMapPropModelFileId(GW::MapProp* prop)
    {
        if (!(prop && prop->h0034[4]))
            return 0;
        uint32_t* sub_deets = (uint32_t*)prop->h0034[4];
        return ArenaNetFileParser::FileHashToFileId((wchar_t*)sub_deets[1]);
    };

    bool IsTravelPortal(GW::MapProp* prop)
    {
        switch (GetMapPropModelFileId(prop)) {
            case 0x4e6b2: // Eotn asura gate
            case 0x3c5ac: // Eotn, Nightfall
            case 0xa825:  // Prophecies, Factions
                return true;
        }
        return false;
    }

    bool IsValidOutpost(GW::Constants::MapID map_id)
    {
        const auto map_info = GW::Map::GetMapInfo(map_id);
        if (!map_info || !map_info->thumbnail_id || !map_info->name_id || !(map_info->x || map_info->y))
            return false;
        if ((map_info->flags & 0x5000000) == 0x5000000)
            return false; // e.g. "wrong" augury rock is map 119, no NPCs
        if ((map_info->flags & 0x80000000) == 0x80000000)
            return false; // e.g. Debug map
        switch (map_info->type) {
            case GW::RegionType::City:
            case GW::RegionType::CompetitiveMission:
            case GW::RegionType::CooperativeMission:
            case GW::RegionType::EliteMission:
            case GW::RegionType::MissionOutpost:
            case GW::RegionType::Outpost:
                break;
            default:
                return false;
        }
        return true;
    }

    struct MapPortal {
        GW::Constants::MapID from;
        GW::Constants::MapID to;
        GW::Vec2f world_pos;
    };

    std::vector<MapPortal> map_portals;

    GW::Constants::MapID GetClosestMapToPoint(const GW::Vec2f& world_map_point)
    {
        for (size_t i = 0; i < (size_t)GW::Constants::MapID::Count; i++) {
            const auto map_info = GW::Map::GetMapInfo((GW::Constants::MapID)i);
            if (!map_info || !map_info->thumbnail_id || !map_info->name_id || !(map_info->x || map_info->y))
                continue;
            if ((map_info->flags & 0x5000000) == 0x5000000)
                continue; // e.g. "wrong" augury rock is map 119, no NPCs
            if ((map_info->flags & 0x80000000) == 0x80000000)
                continue; // e.g. Debug map
            if (!map_info->GetIsOnWorldMap())
                continue;
            (world_map_point);
            // TODO: distance from point to rect
        }
        return GW::Constants::MapID::None;
    }

    GW::MapProp* GetClosestPortalToLocation(const GW::Vec2f& game_pos)
    {
        GW::MapProp* found = nullptr;
        float closest_distance = .9999f;
        const auto props = GW::Map::GetMapProps();
        if (!props)
            return found;
        for (auto prop : *props) {
            if (!IsTravelPortal(prop))
                continue;
            // TOOD: If found is null or this prop->location is closer than the found one, this wins
            // Calculate the distance between the current portal and the given location
            float distance = GW::GetDistance(prop->position, game_pos);

            // If found is null or this portal is closer than the currently found one, update found
            if (!found || distance < closest_distance) {
                found = prop;
                closest_distance = distance;
            }
        }
        return found;
    }

    bool AppendMapPortals()
    {
        const auto props = GW::Map::GetMapProps();
        const auto map_id = GW::Map::GetMapID();
        if (!props) return false;
        for (auto prop : *props) {
            if (IsTravelPortal(prop)) {
                GW::Vec2f world_pos;
                if (!WorldMapWidget::GamePosToWorldMap({prop->position.x, prop->position.y}, world_pos))
                    continue;
                map_portals.push_back({
                    map_id, GetClosestMapToPoint(world_pos), world_pos
                });
            }
        }
        return true;
    }


    GW::HookEntry OnUIMessage_HookEntry;

    void OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void*, void*)
    {
        if (status->blocked)
            return;

        switch (message_id) {
            case GW::UI::UIMessage::kMapLoaded:
                map_portals.clear();
                AppendMapPortals();
                break;
                break;
        }
    }

    void TriggerWorldMapRedraw()
    {
        GW::GameThread::Enqueue([] {
            // Trigger a benign ui message e.g. guild context update; world map subscribes to this, and automatically updates the view.
            // GW::UI::SendUIMessage((GW::UI::UIMessage)0x100000ca); // disables guild/ally chat until reloading char/map
            const auto world_map_context = GW::Map::GetWorldMapContext();
            const auto frame = world_map_context ? GW::UI::GetFrameById(world_map_context->frame_id) : nullptr;
            GW::UI::SendFrameUIMessage(frame, GW::UI::UIMessage::kMapLoaded, nullptr);
            //GW::UI::SendFrameUIMessage(frame,(GW::UI::UIMessage)0x1000008e, nullptr);
        });
    }

    void HighlightFrame(GW::UI::Frame* frame)
    {
        if (!frame) return;
        const auto root = GW::UI::GetRootFrame();
        const auto top_left = frame->position.GetTopLeftOnScreen(root);
        const auto bottom_right = frame->position.GetBottomRightOnScreen(root);
        const auto draw_list = ImGui::GetBackgroundDrawList();
        draw_list->AddRect({top_left.x, top_left.y}, {bottom_right.x, bottom_right.y}, IM_COL32_WHITE);
    }

    // Helper function to calculate rotated points
    void CalculateRotatedPoints(const ImRect& rect, const ImVec2& center, float rotation_angle, ImVec2 out_points[4])
    {
        ImVec2 points[4] = {
            rect.Min,                 // Top-left
            {rect.Max.x, rect.Min.y}, // Top-right
            rect.Max,                 // Bottom-right
            {rect.Min.x, rect.Max.y}  // Bottom-left
        };

        for (int i = 0; i < 4; ++i) {
            const float dx = points[i].x - center.x;
            const float dy = points[i].y - center.y;

            // Apply the rotation transformation using rotation_angle
            out_points[i] = {
                center.x + dx * cos(rotation_angle) - dy * sin(rotation_angle),
                center.y + dx * sin(rotation_angle) + dy * cos(rotation_angle)
            };
        }
    }

    // Helper function to calculate UV coordinates for a sprite map
    void CalculateUVCoords(float uv_start_x, float uv_end_x, ImVec2 uv_points[4])
    {
        uv_points[0] = {uv_start_x, 0.0f}; // Top-left
        uv_points[1] = {uv_end_x, 0.0f};   // Top-right
        uv_points[2] = {uv_end_x, 1.0f};   // Bottom-right
        uv_points[3] = {uv_start_x, 1.0f}; // Bottom-left
    }

    // Cached vars that are updated every draw; avoids having to do the calculation inside DrawQuestMarkerOnWorldMap
    GW::Vec2f player_world_map_pos;
    float player_rotation = .0f;
    GW::Vec2f viewport_offset;
    GW::Vec2f ui_scale;
    float world_map_scale = 1.f;
    GW::WorldMapContext* world_map_context = nullptr;
    float quest_star_rotation_angle = .0f;
    float quest_icon_size = 24.f;
    float quest_icon_size_half = 12.f;
    ImDrawList* draw_list = nullptr;

    // Function to calculate viewport position
    ImVec2 CalculateViewportPos(const GW::Vec2f& marker_world_pos, const ImVec2& top_left)
    {
        return {
            ui_scale.x * world_map_scale * (marker_world_pos.x - top_left.x) + viewport_offset.x,
            ui_scale.y * world_map_scale * (marker_world_pos.y - top_left.y) + viewport_offset.y
        };
    }

    GW::Vec2f GetMapMarkerPoint(GW::AreaInfo* map_info)
    {
        if (!map_info)
            return {};
        if (map_info->x && map_info->y) {
            // If the map has an icon x and y coord, use that as the custom quest marker position
            // NB: GW places this marker at the top of the outpost icon, not the center - probably to make it easier to see? sounds daft, don't copy it.
            return {
                (float)map_info->x,
                (float)map_info->y
            };
        }
        if (map_info->icon_start_x && map_info->icon_start_y) {
            // Otherwise use the center position of the map name label
            return {
                (float)(map_info->icon_start_x + ((map_info->icon_end_x - map_info->icon_start_x) / 2)),
                (float)(map_info->icon_start_y + ((map_info->icon_end_y - map_info->icon_start_y) / 2))
            };
        }
        // Otherwise use the center position of the map name label
        return {
            (float)(map_info->icon_start_x_dupe + ((map_info->icon_end_x_dupe - map_info->icon_start_x_dupe) / 2)),
            (float)(map_info->icon_start_y_dupe + ((map_info->icon_end_y_dupe - map_info->icon_start_y_dupe) / 2))
        };
    }

    // Pre-calculate some cached vars for this frame to avoid having to recalculate more than once
    bool PreCalculateFrameVars()
    {
        world_map_context = GW::Map::GetWorldMapContext();
        if (!world_map_context)
            return false;
        const auto viewport = ImGui::GetMainViewport();
        viewport_offset = viewport->Pos;
        draw_list = ImGui::GetBackgroundDrawList(viewport);
        ui_scale = GW::UI::GetFrameById(world_map_context->frame_id)->position.GetViewportScale(GW::UI::GetRootFrame());

        const auto me = GW::Agents::GetControlledCharacter();
        if (!(me && WorldMapWidget::GamePosToWorldMap(me->pos, player_world_map_pos)))
            return false;
        player_rotation = me->rotation_angle;

        const GW::Vec2f world_map_size_in_coords = {
            (float)world_map_context->h004c[5],
            (float)world_map_context->h004c[6]
        };
        const GW::Vec2f world_map_zoomed_out_size = {
            world_map_context->h0030,
            world_map_context->h0034
        };

        world_map_scale = 1.f;
        if (world_map_context->zoom != 1.0f) {
            // If we're zoomed out, the world map coordinates aren't 1:1 scale; we need to find the scale factor
            if (world_map_context->top_left.y == 0.f) {
                // The zoomed out map fills vertically
                world_map_scale = world_map_zoomed_out_size.y / world_map_size_in_coords.y;
            }
            else {
                // The zoomed out map fills horizontally
                world_map_scale = world_map_zoomed_out_size.x / world_map_size_in_coords.x;
            }
        }
        quest_icon_size = 24.0f * ui_scale.x;
        quest_icon_size_half = quest_icon_size / 2.f;

        constexpr float FULL_ROTATION_TIME = 16.0f;
        const float elapsed_seconds = static_cast<float>(TIMER_INIT()) / CLOCKS_PER_SEC;
        quest_star_rotation_angle = 2.0f * (float)M_PI * fmod(elapsed_seconds, FULL_ROTATION_TIME) / FULL_ROTATION_TIME;

        return true;
    }

    bool DrawQuestMarkerOnWorldMap(const GW::Quest* quest)
    {
        if (!(world_map_context && quest))
            return false;
        if (world_map_context->zoom != 1.f && world_map_context->zoom != .0f)
            return false; // Map is animating


        bool is_hovered = false;
        auto color = GW::QuestMgr::GetActiveQuestId() == quest->quest_id ? 0 : 0x80FFFFFF;
        if (apply_quest_colors) {
            color = QuestModule::GetQuestColor(quest->quest_id);
        }

        // draw_quest_marker
        const auto draw_quest_marker = [&](const GW::Vec2f& quest_marker_pos) {
            const auto viewport_quest_pos = CalculateViewportPos(quest_marker_pos, world_map_context->top_left);

            const ImRect icon_rect = {
                {viewport_quest_pos.x - quest_icon_size_half, viewport_quest_pos.y - quest_icon_size_half},
                {viewport_quest_pos.x + quest_icon_size_half, viewport_quest_pos.y + quest_icon_size_half}
            };

            ImVec2 rotated_points[4];
            CalculateRotatedPoints(icon_rect, viewport_quest_pos, quest_star_rotation_angle, rotated_points);

            ImVec2 uv_points[4];
            CalculateUVCoords(0.0f, 0.5f, uv_points); // Left-hand side of the sprite map

            draw_list->AddImageQuad(
                *quest_icon_texture,
                rotated_points[0], rotated_points[1], rotated_points[2], rotated_points[3],
                uv_points[0], uv_points[1], uv_points[2], uv_points[3], color & IM_COL32_A_MASK ? color : IM_COL32_WHITE
            );

            return icon_rect.Contains(ImGui::GetMousePos());
        };

        // draw_quest_arrow
        const auto draw_quest_arrow = [&](const GW::Vec2f& quest_marker_pos) {
            const auto viewport_quest_pos = CalculateViewportPos(quest_marker_pos, world_map_context->top_left);
            const auto viewport_player_pos = CalculateViewportPos(player_world_map_pos, world_map_context->top_left);
            // Calculate the vector from your position to the quest marker
            const float dx = viewport_quest_pos.x - viewport_player_pos.x;
            const float dy = viewport_quest_pos.y - viewport_player_pos.y;

            // Calculate the rotation angle in radians using atan2, pointing away from the player
            float rotation_angle = std::atan2f(-dy, -dx);
            rotation_angle += DirectX::XM_PI;

            const ImRect icon_rect = {
                {viewport_quest_pos.x - quest_icon_size_half, viewport_quest_pos.y - quest_icon_size_half},
                {viewport_quest_pos.x + quest_icon_size_half, viewport_quest_pos.y + quest_icon_size_half}
            };

            ImVec2 rotated_points[4];
            CalculateRotatedPoints(icon_rect, viewport_quest_pos, rotation_angle, rotated_points);

            ImVec2 uv_points[4];
            CalculateUVCoords(0.5f, 1.0f, uv_points); // Right-hand side of the sprite map

            draw_list->AddImageQuad(
                *quest_icon_texture,
                rotated_points[0], rotated_points[1], rotated_points[2], rotated_points[3], uv_points[0], uv_points[1], uv_points[2], uv_points[3], color & IM_COL32_A_MASK ? color : IM_COL32_WHITE
            );

            return icon_rect.Contains(ImGui::GetMousePos());
        };

        // The quest doesn't end in this map; the marker icon needs to be an arrow, and the actual marker needs to be positioned onto the label of the destination map
        const auto map_info = GW::Map::GetMapInfo(quest->map_to);
        if (!(map_info && map_info->continent == world_map_context->continent))
            return false;
        GW::Vec2f pos;
        if (WorldMapWidget::GamePosToWorldMap(quest->marker, pos)) {
            if (quest->map_to != GW::Map::GetMapID()) {
                is_hovered |= draw_quest_arrow(pos);
            }
            else {
                is_hovered |= draw_quest_marker(pos);
            }
        }
        if (quest->map_to != GW::Map::GetMapID() || world_map_context->zoom == .0f) {
            is_hovered |= draw_quest_marker(GetMapMarkerPoint(map_info));
        }
        return is_hovered;
    }
}

GW::Constants::MapID WorldMapWidget::GetMapIdForLocation(const GW::Vec2f& world_map_pos, GW::Constants::MapID exclude_map_id)
{
    auto map_id = GW::Map::GetMapID();
    auto map_info = GW::Map::GetMapInfo();
    if (!map_info)
        return GW::Constants::MapID::None;
    const auto campaign = map_info->campaign;
    if (map_id != exclude_map_id && MapContainsWorldPos(map_id, world_map_pos, campaign))
        return map_id;
    for (size_t i = 1; i < static_cast<size_t>(GW::Constants::MapID::Count); i++) {
        map_id = static_cast<GW::Constants::MapID>(i);
        if (map_id == exclude_map_id)
            continue;
        map_info = GW::Map::GetMapInfo(map_id);
        if (!(map_info && map_info->GetIsOnWorldMap()))
            continue;
        if (MapContainsWorldPos(map_id, world_map_pos, campaign))
            return map_id;
    }
    return GW::Constants::MapID::None;
}

void WorldMapWidget::Initialize()
{
    ToolboxWidget::Initialize();
    quest_icon_texture = GwDatTextureModule::LoadTextureFromFileId(0x1b4d5);
    player_icon_texture = GwDatTextureModule::LoadTextureFromFileId(0x5d3b);

    uintptr_t address = GW::Scanner::Find("\x8b\x45\xfc\xf7\x40\x10\x00\x00\x01\x00", "xxxxxxxxxx", 0xa);
    if (address) {
        view_all_outposts_patch.SetPatch(address, "\xeb", 1);
    }
    address = GW::Scanner::Find("\x8b\xd8\x83\xc4\x10\x8b\xcb\x8b\xf3\xd1\xe9", "xxxxxxxxxxx", -0x5);
    if (address) {
        view_all_carto_areas_patch.SetRedirect(address, GetCartographyFlagsForArea);
    }

    ASSERT(view_all_outposts_patch.IsValid());
    ASSERT(view_all_carto_areas_patch.IsValid());

    const GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kQuestAdded,
        GW::UI::UIMessage::kSendSetActiveQuest,
        GW::UI::UIMessage::kMapLoaded,
        GW::UI::UIMessage::kOnScreenMessage,
        GW::UI::UIMessage::kSendAbandonQuest
    };
    for (auto ui_message : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&OnUIMessage_HookEntry, ui_message, OnUIMessage);
    }
}

bool WorldMapWidget::WorldMapToGamePos(const GW::Vec2f& world_map_pos, GW::GamePos& game_map_pos)
{
    ImRect map_bounds;
    if (!GW::Map::GetMapWorldMapBounds(GW::Map::GetMapInfo(), &map_bounds))
        return false;

    const auto current_map_context = GW::GetMapContext();
    if (!current_map_context)
        return false;

    const auto game_map_rect = ImRect({
        current_map_context->map_boundaries[1], current_map_context->map_boundaries[2],
        current_map_context->map_boundaries[3], current_map_context->map_boundaries[4],
    });

    constexpr auto gwinches_per_unit = 96.f;

    // Calculate the mid-point of the map in world coordinates
    GW::Vec2f map_mid_world_point = {
        map_bounds.Min.x + (abs(game_map_rect.Min.x) / gwinches_per_unit),
        map_bounds.Min.y + (abs(game_map_rect.Max.y) / gwinches_per_unit),
    };

    // Convert from world map position to game map position
    game_map_pos.x = (world_map_pos.x - map_mid_world_point.x) * gwinches_per_unit;
    game_map_pos.y = (world_map_pos.y - map_mid_world_point.y) * gwinches_per_unit * -1.f; // Invert Y axis

    return true;
}

bool WorldMapWidget::GamePosToWorldMap(const GW::GamePos& game_map_pos, GW::Vec2f& world_map_pos)
{
    if (game_map_pos.x == INFINITY || game_map_pos.y == INFINITY)
        return false;
    ImRect map_bounds;
    if (!GW::Map::GetMapWorldMapBounds(GW::Map::GetMapInfo(), &map_bounds))
        return false;
    const auto current_map_context = GW::GetMapContext();
    if (!current_map_context)
        return false;

    const auto game_map_rect = ImRect({
        current_map_context->map_boundaries[1], current_map_context->map_boundaries[2],
        current_map_context->map_boundaries[3], current_map_context->map_boundaries[4],
    });

    // NB: World map is 96 gwinches per unit, this is hard coded in the GW source
    constexpr auto gwinches_per_unit = 96.f;
    GW::Vec2f map_mid_world_point = {
        map_bounds.Min.x + (abs(game_map_rect.Min.x) / gwinches_per_unit),
        map_bounds.Min.y + (abs(game_map_rect.Max.y) / gwinches_per_unit),
    };

    world_map_pos.x = (game_map_pos.x / gwinches_per_unit) + map_mid_world_point.x;
    world_map_pos.y = ((game_map_pos.y * -1.f) / gwinches_per_unit) + map_mid_world_point.y; // Inverted Y Axis
    return true;
}

void WorldMapWidget::SignalTerminate()
{
    ToolboxWidget::Terminate();

    view_all_outposts_patch.Reset();
    view_all_carto_areas_patch.Reset();
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_HookEntry);
}

void WorldMapWidget::ShowAllOutposts(const bool show = showing_all_outposts)
{
    if (view_all_outposts_patch.IsValid())
        view_all_outposts_patch.TogglePatch(show);
    if (view_all_carto_areas_patch.IsValid())
        view_all_carto_areas_patch.TogglePatch(show);
    TriggerWorldMapRedraw();
}

void WorldMapWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    LOAD_BOOL(showing_all_outposts);
    LOAD_BOOL(show_lines_on_world_map);
    LOAD_BOOL(showing_all_quests);
    LOAD_BOOL(apply_quest_colors);
    ShowAllOutposts(showing_all_outposts);
}

void WorldMapWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_BOOL(showing_all_outposts);
    SAVE_BOOL(show_lines_on_world_map);
    SAVE_BOOL(showing_all_quests);
    SAVE_BOOL(apply_quest_colors);
}

void WorldMapWidget::Draw(IDirect3DDevice9*)
{
    if (!(GW::UI::GetIsWorldMapShowing() && PreCalculateFrameVars())) {
        //ShowAllOutposts(showing_all_outposts = false);
        drawn = false;
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowPos(ImVec2(16.f, 16.f), ImGuiCond_FirstUseEver);
    visible = true;
    ImGuiWindow* window = nullptr;
    auto mouse_offset = viewport_offset;
    mouse_offset.x *= -1;
    mouse_offset.y *= -1;
    if (ImGui::Begin(Name(), &visible, GetWinFlags() | ImGuiWindowFlags_AlwaysAutoResize)) {
        window = ImGui::GetCurrentWindowRead();
        if (ImGui::Checkbox("Show all areas", &showing_all_outposts)) {
            GW::GameThread::Enqueue([] {
                ShowAllOutposts(showing_all_outposts);
            });
        }
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
            bool is_hard_mode = GW::PartyMgr::GetIsPartyInHardMode();
            if (ImGui::Checkbox("Hard mode", &is_hard_mode)) {
                GW::GameThread::Enqueue([] {
                    GW::PartyMgr::SetHardMode(!GW::PartyMgr::GetIsPartyInHardMode());
                });
            }
        }
        ImGui::Checkbox("Show toolbox minimap lines", &show_lines_on_world_map);
        if (ImGui::Checkbox("Show quest markers for all quests", &showing_all_quests)) {
            QuestModule::FetchMissingQuestInfo();
        }
        ImGui::Checkbox("Apply quest marker color overlays", &apply_quest_colors);
        if (apply_quest_colors) {
            ImGui::Indent();
            auto color = &QuestModule::GetQuestColor((GW::Constants::QuestID)0xfff);
            ImGui::ColorButtonPicker("Other Quests", color, ImGuiColorEditFlags_NoLabel);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Color overlay for quests that aren't active.");
            }
            if (GW::QuestMgr::GetActiveQuestId() != GW::Constants::QuestID::None) {
                ImGui::SameLine();
                color = &QuestModule::GetQuestColor(GW::QuestMgr::GetActiveQuestId());
                ImGui::ColorButtonPicker("Active Quest", color, ImGuiColorEditFlags_NoLabel);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Color overlay for the active quest.");
                }
            }
            ImGui::Unindent();
        }
    }

    
    ImGui::End();
    ImGui::PopStyleColor();

    if (window) {
        controls_window_rect = window->Rect();
        controls_window_rect.Translate(mouse_offset);
    }

    AppendMapPortals();

    hovered_quest_id = GW::Constants::QuestID::None;
    // Draw all quest markers on world map if applicable
    if (showing_all_quests) {
        if (const auto quest_log = GW::QuestMgr::GetQuestLog()) {
            for (auto& quest : *quest_log) {
                if (DrawQuestMarkerOnWorldMap(&quest)) {
                    hovered_quest_id = quest.quest_id;
                }
            }
        }
    }
    const auto active = GW::QuestMgr::GetActiveQuest();
    if (DrawQuestMarkerOnWorldMap(active)) {
        hovered_quest_id = active->quest_id;
    }
    if (hovered_quest_id != GW::Constants::QuestID::None) {
        if (const auto hovered_quest = GW::QuestMgr::GetQuest(hovered_quest_id)) {
            static GuiUtils::EncString quest_name;
            if (!quest_name.IsDecoding())
                quest_name.reset(hovered_quest->name);
            ImGui::SetTooltip("%s", quest_name.string().c_str());
        }
    }

    /*for (const auto& portal : map_portals) {
        static constexpr auto uv0 = ImVec2(0.0f, 0.0f);
        static constexpr auto ICON_SIZE = ImVec2(24.0f, 24.0f);

        const ImVec2 portal_pos = {
            ui_scale.x * (portal.world_pos.x - world_map_context->top_left.x) + viewport_offset.x - (ICON_SIZE.x / 2.f),
            ui_scale.y * (portal.world_pos.y - world_map_context->top_left.y) + viewport_offset.y - (ICON_SIZE.y / 2.f)
        };

        const ImRect hover_rect = {
            portal_pos, {portal_pos.x + ICON_SIZE.x, portal_pos.y + ICON_SIZE.y}
        };

        draw_list->AddImage(*GwDatTextureModule::LoadTextureFromFileId(0x1b4d5), hover_rect.GetTL(), hover_rect.GetBR());


        if (hover_rect.Contains(ImGui::GetMousePos())) {
            ImGui::SetTooltip("Portal");
        }
    }*/
    if (show_lines_on_world_map) {
        const auto& lines = Minimap::Instance().custom_renderer.GetLines();
        const auto map_id = GW::Map::GetMapID();
        GW::Vec2f line_start;
        GW::Vec2f line_end;
        for (auto& line : lines | std::views::filter([](auto line) { return line->visible; })) {
            if (line->map != map_id)
                continue;
            if (!GamePosToWorldMap(line->p1, line_start))
                continue;
            if (!GamePosToWorldMap(line->p2, line_end))
                continue;

            line_start.x = (line_start.x - world_map_context->top_left.x) * ui_scale.x + viewport_offset.x;
            line_start.y = (line_start.y - world_map_context->top_left.y) * ui_scale.y + viewport_offset.y;
            line_end.x = (line_end.x - world_map_context->top_left.x) * ui_scale.x + viewport_offset.x;
            line_end.y = (line_end.y - world_map_context->top_left.y) * ui_scale.y + viewport_offset.y;

            draw_list->AddLine(line_start, line_end, line->color);
        }
    }
    drawn = true;
}

bool WorldMapWidget::WndProc(const UINT Message, WPARAM, LPARAM lParam)
{
    auto check_rect = [lParam](const ImRect& rect) {
        ImVec2 p = {(float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam)};
        return rect.Contains(p);
    };

    switch (Message) {
        case WM_GW_RBUTTONCLICK: {
            if (!(world_map_context && GW::UI::GetIsWorldMapShowing()))
                break;
            if (GW::QuestMgr::GetQuest(hovered_quest_id)) {
                ImGui::SetContextMenu(HoveredQuestContextMenu, (void*)hovered_quest_id);
                break;
            }
            if (!(world_map_context && world_map_context->zoom == 1.0f))
                break;
            world_map_click_pos = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            world_map_click_pos.x /= ui_scale.x;
            world_map_click_pos.y /= ui_scale.y;
            world_map_click_pos.x += world_map_context->top_left.x;
            world_map_click_pos.y += world_map_context->top_left.y;
            ImGui::SetContextMenu(WorldMapContextMenu);
        }
        break;
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_LBUTTONDOWN:
            if (!drawn || !GW::UI::GetIsWorldMapShowing())
                return false;
            if (ImGui::ShowingContextMenu())
                return true;
            if (check_rect(controls_window_rect))
                return true;
            break;
    }
    return false;
}

void WorldMapWidget::DrawSettingsInternal()
{
    ImGui::Text("Note: only visible in Hard Mode explorable areas.");
}
