#include "stdafx.h"

#include <fstream>
#include <sstream>

#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Pathing.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Context/MapContext.h>

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Modules/GwDatTextureModule.h>
#include <Modules/Resources.h>
#include <Widgets/Minimap/Minimap.h>

#include <Widgets/WorldMapWidget.h>
#include <Widgets/WorldMapWidget_Constants.h>

#include <Windows/CompletionWindow.h>
#include <Windows/TravelWindow.h>

#include <Utils/GuiUtils.h>
#include <Utils/ToolboxUtils.h>

#include "Defines.h"

#include <GWCA/Managers/AgentMgr.h>
#include <ImGuiAddons.h>
#include <Modules/QuestModule.h>
#include <Utils/ArenaNetFileParser.h>
#include <Utils/TextUtils.h>
#include <corecrt_math_defines.h>
#include <Windows/Pathfinding/PathingMapDataLoader.h>




namespace {
    using namespace WorldMapWidget_Constants;

    struct MapPortal;

    uint32_t current_map_file_id = 0;
    GW::Constants::MapID current_map_id = GW::Constants::MapID::None;

    struct MapFileInfo {
        GW::Continent continent;
        GW::Vec2f world_pos_start; // top left of bounds
        GW::Vec2f world_pos_end;   // bottom right of bounds
        uint32_t map_file_id;      // unique identifier for this map
        GW::Constants::MapID map_id = GW::Constants::MapID::None;
        std::vector<MapPortal> portals;
    };

    std::map<uint32_t, MapFileInfo> map_info_by_file_id;


    struct MapPortal {
        GW::Vec2f world_pos;
        uint32_t map_file_id = 0;
        uint32_t prop_index = 0;
        
        uint32_t linked_portal_map_file_id = 0;
        uint32_t linked_portal_prop_index = 0;

        const MapPortal* linkedPortal() const
        {
            const auto found = map_info_by_file_id.find(linked_portal_map_file_id);
            if (found == map_info_by_file_id.end()) return nullptr;

            const auto& other_map_portals = found->second.portals;

            const auto other_portal = std::ranges::find_if(other_map_portals.begin(), other_map_portals.end(), [this](const MapPortal& other) {
                return other.linked_portal_prop_index == this->prop_index;
            });
            return other_portal == other_map_portals.end() ? nullptr : &other_portal[0];
        }

        void checkForLinkedPortal(GW::Continent continent)
        {
            if (linked_portal_map_file_id) return;
            for (auto& it : map_info_by_file_id) {
                if (it.second.continent != continent) continue;
                auto& other_map_portals = it.second.portals;
                auto other_portal = std::ranges::find_if(other_map_portals.begin(), other_map_portals.end(), [this](const MapPortal& other) {
                    return other.map_file_id != this->map_file_id && other.world_pos.x == this->world_pos.x && other.world_pos.y == this->world_pos.y;
                });
                if (other_portal != other_map_portals.end()) {
                    linked_portal_map_file_id = other_portal->map_file_id;
                    linked_portal_prop_index = other_portal->prop_index;
                    other_portal->linked_portal_map_file_id = map_file_id;
                    other_portal->linked_portal_prop_index = prop_index;
                    return;
                }
            }
        }
    };





    const ImColor completed_bg = IM_COL32(0, 0x99, 0, 192);
    const ImColor completed_text = IM_COL32(0xE5, 0xFF, 0xCC, 255);

    ImRect controls_window_rect = {0, 0, 0, 0};

    IDirect3DTexture9** quest_icon_texture = nullptr;
    IDirect3DTexture9** player_icon_texture = nullptr;
    IDirect3DTexture9** portal_icon_texture = nullptr;

    bool showing_all_outposts = false;
    bool apply_quest_colors = false;
    bool show_any_elite_capture_locations = false;
    bool show_elite_capture_locations[11];
    bool hide_captured_elites = false;
    bool drawn = false;
    bool show_lines_on_world_map = false;
    bool showing_all_quests = true;

    GW::MemoryPatcher view_all_outposts_patch;
    GW::MemoryPatcher view_all_carto_areas_patch;

    bool world_map_clicking = false;
    GW::Vec2f world_map_click_pos;

    GW::Constants::QuestID hovered_quest_id = GW::Constants::QuestID::None;
    GuiUtils::EncString hovered_quest_name;
    GuiUtils::EncString hovered_quest_description;
    const EliteBossLocation* hovered_boss = nullptr;
    const MapPortal* hovered_map_portal = nullptr;

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

    std::string BossInfo(const EliteBossLocation* boss)
    {
        const auto map_info = GW::Map::GetMapInfo(boss->map_id);
        const char* mission_suffix = "";
        switch (map_info->type) {
            case GW::RegionType::MissionOutpost:
            case GW::RegionType::EotnMission:
            case GW::RegionType::CooperativeMission:
                mission_suffix = " (Mission)";
                break;
            case GW::RegionType::Challenge:
                mission_suffix = " (Challenge)";
                break;
        }

        auto str = std::format("{} - {}\n{}{}", boss->boss_name, Resources::GetSkillName(boss->skill_id)->string(), Resources::GetMapName(boss->map_id)->string(), mission_suffix);
        if (boss->note) {
            str += std::format("\n{}", boss->note);
        }
        return str;
    }
    void DrawMapPortalInfo(const MapPortal* portal, bool include_linked = true)
    {
        auto& world_map_pos = portal->world_pos;

        ImGui::Text("%.2f, %.2f", world_map_pos.x, world_map_pos.y);
#ifdef _DEBUG
        GW::GamePos game_pos;
        if (WorldMapWidget::WorldMapToGamePos(world_map_pos, game_pos)) {
            ImGui::Text("%.2f, %.2f", game_pos.x, game_pos.y);
        }
#endif

        ImGui::Text("Prop Index: %d", portal->prop_index);
        ImGui::Text("Map File ID: %d", portal->map_file_id);
        if (include_linked) {
            if (const auto linked = portal->linkedPortal()) {
                ImGui::Text("Linked with:");
                ImGui::Separator();
                DrawMapPortalInfo(linked, false);
            }
        }
    }

    uint32_t __cdecl GetCartographyFlagsForArea(uint32_t, uint32_t, uint32_t, uint32_t)
    {
        return 0xffffffff;
    }

    bool MapContainsWorldPos(GW::Constants::MapID map_id, const GW::Vec2f& world_map_pos, GW::Continent continent)
    {
        const auto map = GW::Map::GetMapInfo(map_id);
        if (!(map && map->continent == continent)) return false;
        ImRect map_bounds;
        return GW::Map::GetMapWorldMapBounds(map, &map_bounds) && map_bounds.Contains(world_map_pos);
    }

    bool ContextMenuMarkerButtons()
    {
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

    bool WorldMapContextMenu(void*)
    {
        if (!GW::Map::GetWorldMapContext()) return false;

        ImGui::Text("%.2f, %.2f", world_map_click_pos.x, world_map_click_pos.y);
#ifdef _DEBUG
        GW::GamePos game_pos;
        if (WorldMapWidget::WorldMapToGamePos(world_map_click_pos, game_pos)) {
            ImGui::Text("%.2f, %.2f", game_pos.x, game_pos.y);
        }
#endif
        const auto map_id = WorldMapWidget::GetMapIdForLocation(world_map_click_pos);
        ImGui::TextUnformatted(Resources::GetMapName(map_id)->string().c_str());

        if (!ContextMenuMarkerButtons()) return false;
        return true;
    }

    bool HoveredQuestContextMenu(void* wparam)
    {
        if (!GW::Map::GetWorldMapContext()) return false;
        const auto quest_id = static_cast<GW::Constants::QuestID>(reinterpret_cast<uint32_t>(wparam));
        const auto quest = GW::QuestMgr::GetQuest(quest_id);
        if (!quest) return false;
        if (!hovered_quest_name.IsDecoding()) hovered_quest_name.reset(quest->name);
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
        ImGui::Separator();
        if (!ContextMenuMarkerButtons()) return false;

        if (set_active) {
            GW::GameThread::Enqueue([quest_id] {
                GW::QuestMgr::SetActiveQuestId(quest_id);
            });
            return false;
        }
        if (travel) {
            if (TravelWindow::Instance().TravelNearest(quest->map_to)) return false;
        }
        if (wiki) {
            GW::GameThread::Enqueue([quest_id] {
                if (GW::QuestMgr::GetQuest(quest_id)) {
                    const auto wiki_url = std::format("{}Game_link:Quest_{}", GuiUtils::WikiUrl(L""), static_cast<uint32_t>(quest_id));
                    SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, (void*)wiki_url.c_str());
                }
            });
            return false;
        }
        return true;
    }

    bool EliteBossLocationContextMenu(void* wparam)
    {
        if (!GW::Map::GetWorldMapContext()) return false;
        const auto boss = (EliteBossLocation*)wparam;
        if (!boss) return false;
        ImGui::Text("%s", BossInfo(boss).c_str());

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImColor(0, 0, 0, 0).Value);
        const auto size = ImVec2(250.0f * ImGui::GetIO().FontGlobalScale, 0);
        ImGui::Separator();

        const bool travel = ImGui::Button("Travel to nearest outpost", size);

        const auto boss_label = std::format("{} on Guild Wars Wiki", boss->boss_name);
        const bool boss_wiki = ImGui::Button(boss_label.c_str(), size);

        const auto skill_label = std::format("{} on Guild Wars Wiki", Resources::GetSkillName(boss->skill_id)->string());
        const bool skill_wiki = ImGui::Button(skill_label.c_str(), size);

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        ImGui::Separator();
        if (!ContextMenuMarkerButtons()) return false;


        if (travel) {
            if (TravelWindow::Instance().TravelNearest(boss->map_id)) return false;
        }
        if (boss_wiki) {
            GuiUtils::SearchWiki(TextUtils::StringToWString(boss->boss_name));
            return false;
        }
        if (skill_wiki) {
            GW::GameThread::Enqueue([boss] {
                const auto wiki_url = std::format("{}Game_link:Skill_{}", GuiUtils::WikiUrl(L""), static_cast<uint32_t>(boss->skill_id));
                SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, (void*)wiki_url.c_str());
            });
            return false;
        }
        return true;
    }

    bool MapPortalContextMenu(void* wparam)
    {
        if (!GW::Map::GetWorldMapContext()) return false;
        const auto portal = (MapPortal*)wparam;
        if (!portal) return false;

        DrawMapPortalInfo(portal);
        return true;
    }

    uint32_t GetMapPropModelFileId(GW::MapProp* prop)
    {
        if (!(prop && prop->h0034[4])) return 0;
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
        if (!map_info || !map_info->thumbnail_id || !map_info->name_id || !(map_info->x || map_info->y)) return false;
        if ((map_info->flags & 0x5000000) == 0x5000000) return false;   // e.g. "wrong" augury rock is map 119, no NPCs
        if ((map_info->flags & 0x80000000) == 0x80000000) return false; // e.g. Debug map
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

    GW::Constants::MapID GetClosestMapToPoint(const GW::Vec2f& world_map_point)
    {
        for (size_t i = 0; i < (size_t)GW::Constants::MapID::Count; i++) {
            const auto map_info = GW::Map::GetMapInfo((GW::Constants::MapID)i);
            if (!map_info || !map_info->thumbnail_id || !map_info->name_id || !(map_info->x || map_info->y)) continue;
            if ((map_info->flags & 0x5000000) == 0x5000000) continue;   // e.g. "wrong" augury rock is map 119, no NPCs
            if ((map_info->flags & 0x80000000) == 0x80000000) continue; // e.g. Debug map
            if (!map_info->GetIsOnWorldMap()) continue;
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
        if (!props) return found;
        for (auto prop : *props) {
            if (!IsTravelPortal(prop)) continue;
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

    void AppendMapFileInfo()
    {
        if (!current_map_file_id || map_info_by_file_id.contains(current_map_file_id)) 
            return;
        MapFileInfo info;
        const auto map_context = GW::GetMapContext();
        info.map_file_id = current_map_file_id;
        info.map_id = map_context->map_id;
        info.continent = GW::Map::GetMapInfo(info.map_id)->continent;

        std::vector<MapPortal> portals;
        const auto props = GW::Map::GetMapProps();

        if (!props) return;
        for (auto prop : *props) {
            if (IsTravelPortal(prop)) {
                GW::Vec2f world_pos;
                if (!WorldMapWidget::GamePosToWorldMap({prop->position.x, prop->position.y}, world_pos)) continue;
                portals.push_back({world_pos, current_map_file_id, prop->prop_index});
            }
        }
        for (auto& portal : portals) {
            portal.checkForLinkedPortal(info.continent);
        }

        WorldMapWidget::GamePosToWorldMap(map_context->start_pos, info.world_pos_start);
        WorldMapWidget::GamePosToWorldMap(map_context->end_pos, info.world_pos_end);
        info.portals = std::move(portals);

        map_info_by_file_id[current_map_file_id] = info;
    }


    GW::HookEntry OnUIMessage_HookEntry;

    void OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wParam, void*)
    {
        if (status->blocked) return;
        switch (message_id) {
            case GW::UI::UIMessage::kLoadMapContext: {
                const auto packet = (GW::UI::UIPacket::kLoadMapContext*)wParam;
                current_map_file_id = 0;
                current_map_id = GW::Constants::MapID::None;
                if (packet->file_name && *packet->file_name) {
                    current_map_file_id = ArenaNetFileParser::FileHashToFileId(packet->file_name);
                }
                AppendMapFileInfo();
                QuestModule::FetchMissingQuestInfo();
            } break;
        }
    }

    void TriggerWorldMapRedraw()
    {
        GW::GameThread::Enqueue([] {
            const auto ctx = GW::Map::GetWorldMapContext();
            const auto frame = ctx ? GW::UI::GetFrameById(ctx->frame_id) : nullptr;
            GW::UI::DestroyUIComponent(frame) && GW::UI::Keypress(GW::UI::ControlAction_OpenWorldMap), true;
        });
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
            out_points[i] = {center.x + dx * cos(rotation_angle) - dy * sin(rotation_angle), center.y + dx * sin(rotation_angle) + dy * cos(rotation_angle)};
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



    // Function to calculate viewport position
    ImVec2 CalculateViewportPos(const GW::Vec2f& marker_world_pos, const ImVec2& top_left)
    {
        return {ui_scale.x * world_map_scale * (marker_world_pos.x - top_left.x) + viewport_offset.x, ui_scale.y * world_map_scale * (marker_world_pos.y - top_left.y) + viewport_offset.y};
    }

    GW::Vec2f GetMapMarkerPoint(GW::AreaInfo* map_info)
    {
        if (!map_info) return {};
        if (map_info->x && map_info->y) {
            // If the map has an icon x and y coord, use that as the custom quest marker position
            // NB: GW places this marker at the top of the outpost icon, not the center - probably to make it easier to see? sounds daft, don't copy it.
            return {(float)map_info->x, (float)map_info->y};
        }
        if (map_info->icon_start_x && map_info->icon_start_y) {
            // Otherwise use the center position of the map name label
            return {(float)(map_info->icon_start_x + ((map_info->icon_end_x - map_info->icon_start_x) / 2)), (float)(map_info->icon_start_y + ((map_info->icon_end_y - map_info->icon_start_y) / 2))};
        }
        // Otherwise use the center position of the map name label
        return {(float)(map_info->icon_start_x_dupe + ((map_info->icon_end_x_dupe - map_info->icon_start_x_dupe) / 2)), (float)(map_info->icon_start_y_dupe + ((map_info->icon_end_y_dupe - map_info->icon_start_y_dupe) / 2))};
    }

    // Pre-calculate some cached vars for this frame to avoid having to recalculate more than once
    bool PreCalculateFrameVars()
    {
        world_map_context = GW::Map::GetWorldMapContext();
        if (!world_map_context) return false;
        const auto viewport = ImGui::GetMainViewport();
        viewport_offset = viewport->Pos;
        draw_list = ImGui::GetBackgroundDrawList(viewport);
        ui_scale = GW::UI::GetFrameById(world_map_context->frame_id)->position.GetViewportScale(GW::UI::GetRootFrame());

        const auto me = GW::Agents::GetControlledCharacter();
        if (!(me && WorldMapWidget::GamePosToWorldMap(me->pos, player_world_map_pos))) return false;
        player_rotation = me->rotation_angle;

        const GW::Vec2f world_map_size_in_coords = {(float)world_map_context->h004c[5], (float)world_map_context->h004c[6]};
        const GW::Vec2f world_map_zoomed_out_size = {world_map_context->h0030, world_map_context->h0034};

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

    GW::Vec2f southern_shivers_start = {4570.f, 4230.f};
    GW::Vec2f post_searing_region = {5705.f, 3590.f};
    GW::Vec2f maguuma_region = {220.f, 4150.f};
    GW::Vec2f crystal_desert_region = {6105.f, 6735.f};
    GW::Vec2f ring_of_fire_region = {1070.f, 7190.f};
    GW::Vec2f kryta_region = {2150.f, 3800.f};

    GW::Vec2f shing_jea_island = {700.f, 2050.f};
    GW::Vec2f kaineng_city = {2430.f, 590.f};
    GW::Vec2f echovald_forest = {3220.f, 2120.f};
    GW::Vec2f jade_sea = {3790.f, 1925.f};

    GW::Vec2f istan = {625.f, 2660.f};
    GW::Vec2f kourna = {2760.f, 1645.f};
    GW::Vec2f vabbi = {3230.f, 360.f};
    GW::Vec2f desolation = {1860.f, 87.f};

    GW::Vec2f far_shiverpeaks = {4470.f, 810.f};
    GW::Vec2f charr_homelands = {6640.f, 1300.f};
    GW::Vec2f tarnished_coast = {1210.f, 5770.f};

    ImVec2 skill_texture_size = {};

    float default_scale = 1.37f;

    std::unordered_map<GW::Constants::MapID, uint32_t> locations_assigned_to_outposts;

    bool DrawBossLocationOnWorldMap(const EliteBossLocation& boss)
    {
        if (!show_any_elite_capture_locations) return false;
        if (!(world_map_context)) return false;
        if (world_map_context->zoom != 1.f && world_map_context->zoom != .0f) return false; // Map is animating

        const auto map_info = GW::Map::GetMapInfo(boss.map_id);
        if (!(map_info && map_info->continent == world_map_context->continent)) return false;

        const auto skill = GW::SkillbarMgr::GetSkillConstantData(boss.skill_id);
        if (!skill) return false;
        if (!show_elite_capture_locations[(uint32_t)skill->profession]) return false;
        if (hide_captured_elites) {
            const auto me = GW::Agents::GetControlledCharacter();
            if (me->primary == (uint8_t)skill->profession || me->secondary == (uint8_t)skill->profession) {
                if (GW::SkillbarMgr::GetIsSkillLearnt(boss.skill_id)) return false;
            }
            else {
                const auto my_name = GW::PlayerMgr::GetPlayerName();
                const auto& completion = CompletionWindow::Instance().GetCharacterCompletion(my_name, false);
                if (completion) {
                    if (CompletionWindow::IsSkillUnlocked(my_name, skill->skill_id)) return false;
                }
                else
                    return false;
            }
        }

        const auto texture = Resources::GetSkillImage(boss.skill_id);
        // const auto texture = Resources::GetProfessionIcon((GW::Constants::Profession)skill->profession);

        if (!(texture && *texture)) return false;

        if (!Resources::GetTextureSize(*texture, &skill_texture_size)) return false;

        float icon_size = world_map_context->zoom == 1.f ? 32.f : 16.f;
        const auto half_size = icon_size / 2.f;

        bool hovered = false;
        float elites_scale = default_scale;
        for (auto boss_pos : boss.coords) {
            GW::Vec2f* region_offset = nullptr;
            switch (map_info->campaign) {
                case GW::Constants::Campaign::Prophecies: {
                    switch (boss.region_id) {
                        case 1: {
                            region_offset = &crystal_desert_region;
                        } break;
                        case 2: {
                            region_offset = &southern_shivers_start;
                        } break;
                        case 3: {
                            region_offset = &ring_of_fire_region;
                        } break;
                        case 4: {
                            region_offset = &post_searing_region;
                        } break;
                        case 5: {
                            region_offset = &kryta_region;
                        } break;
                        case 7: {
                            region_offset = &maguuma_region;
                        } break;
                    }
                } break;
                case GW::Constants::Campaign::Factions: {
                    switch (boss.region_id) {
                        case 1: {
                            region_offset = &kaineng_city;
                        } break;
                        case 2: {
                            region_offset = &echovald_forest;
                        } break;
                        case 3: {
                            region_offset = &jade_sea;
                        } break;
                        case 4: {
                            region_offset = &shing_jea_island;
                        } break;
                    }
                } break;
                case GW::Constants::Campaign::Nightfall: {
                    switch (boss.region_id) {
                        case 1: {
                            region_offset = &istan;
                        } break;
                        case 2: {
                            region_offset = &kourna;
                        } break;
                        case 3: {
                            region_offset = &vabbi;
                            elites_scale = 1.34f;
                        } break;
                        case 4: {
                            region_offset = &desolation;
                            elites_scale = 1.34f;
                        } break;
                        case 5: {
                            // Domain of Anguish is where the devs just gave up trying to make the world map useful.
                            boss_pos = GW::Vec2f((float)map_info->x - icon_size * 2.f, (float)map_info->y);
                            locations_assigned_to_outposts[boss.map_id]++;
                            boss_pos.x += icon_size * (locations_assigned_to_outposts[boss.map_id] - 1);
                            elites_scale = 1.f;
                        }
                    }
                } break;
                case GW::Constants::Campaign::EyeOfTheNorth: {
                    elites_scale = 1.315f;
                    switch (boss.region_id) {
                        case 1: {
                            region_offset = &far_shiverpeaks;
                        } break;
                        case 2: {
                            region_offset = &charr_homelands;
                        } break;
                        case 3: {
                            region_offset = &tarnished_coast;
                            if (boss.map_id == GW::Constants::MapID::Sparkfly_Swamp) {
                                // MappingOut stitches the tarnished coast together, which means I had to manually place the sparkfly swamp elites myself. No need to offset or scale
                                region_offset = nullptr;
                                elites_scale = 1.f;
                            }
                        } break;
                    }
                } break;
            }
            if (boss.region_id == 0xff) {
                boss_pos = GW::Vec2f((float)map_info->x - icon_size * 2.f, (float)map_info->y);
                locations_assigned_to_outposts[boss.map_id]++;
                boss_pos.x += icon_size * (locations_assigned_to_outposts[boss.map_id] - 1);
                elites_scale = 1.f;
            }

            boss_pos *= elites_scale;
            if (region_offset) {
                boss_pos += *region_offset;
            }


            const auto viewport_quest_pos = CalculateViewportPos(boss_pos, world_map_context->top_left);


            const ImRect icon_rect = {{viewport_quest_pos.x - half_size, viewport_quest_pos.y - half_size}, {viewport_quest_pos.x + half_size, viewport_quest_pos.y + half_size}};

            ImGui::AddImageScaled(draw_list, *texture, icon_rect.Min, skill_texture_size, icon_size, icon_size);
            hovered |= icon_rect.Contains(ImGui::GetMousePos());
        }

        return hovered;
    }

    bool DrawQuestMarkerOnWorldMap(const GW::Quest* quest)
    {
        if (!(world_map_context && quest)) return false;
        if (world_map_context->zoom != 1.f && world_map_context->zoom != .0f) return false; // Map is animating


        bool is_hovered = false;
        auto color = GW::QuestMgr::GetActiveQuestId() == quest->quest_id ? 0 : 0x80FFFFFF;
        if (apply_quest_colors) {
            color = QuestModule::GetQuestColor(quest->quest_id);
        }

        // draw_quest_marker
        const auto draw_quest_marker = [&](const GW::Vec2f& quest_marker_pos) {
            const auto viewport_quest_pos = CalculateViewportPos(quest_marker_pos, world_map_context->top_left);

            const ImRect icon_rect = {{viewport_quest_pos.x - quest_icon_size_half, viewport_quest_pos.y - quest_icon_size_half}, {viewport_quest_pos.x + quest_icon_size_half, viewport_quest_pos.y + quest_icon_size_half}};

            ImVec2 rotated_points[4];
            CalculateRotatedPoints(icon_rect, viewport_quest_pos, quest_star_rotation_angle, rotated_points);

            ImVec2 uv_points[4];
            CalculateUVCoords(0.0f, 0.5f, uv_points); // Left-hand side of the sprite map

            draw_list->AddImageQuad(*quest_icon_texture, rotated_points[0], rotated_points[1], rotated_points[2], rotated_points[3], uv_points[0], uv_points[1], uv_points[2], uv_points[3], color & IM_COL32_A_MASK ? color : IM_COL32_WHITE);

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

            const ImRect icon_rect = {{viewport_quest_pos.x - quest_icon_size_half, viewport_quest_pos.y - quest_icon_size_half}, {viewport_quest_pos.x + quest_icon_size_half, viewport_quest_pos.y + quest_icon_size_half}};

            ImVec2 rotated_points[4];
            CalculateRotatedPoints(icon_rect, viewport_quest_pos, rotation_angle, rotated_points);

            ImVec2 uv_points[4];
            CalculateUVCoords(0.5f, 1.0f, uv_points); // Right-hand side of the sprite map

            draw_list->AddImageQuad(*quest_icon_texture, rotated_points[0], rotated_points[1], rotated_points[2], rotated_points[3], uv_points[0], uv_points[1], uv_points[2], uv_points[3], color & IM_COL32_A_MASK ? color : IM_COL32_WHITE);

            return icon_rect.Contains(ImGui::GetMousePos());
        };

        // The quest doesn't end in this map; the marker icon needs to be an arrow, and the actual marker needs to be positioned onto the label of the destination map
        const auto map_info = GW::Map::GetMapInfo(quest->map_to);
        if (!(map_info && map_info->continent == world_map_context->continent)) return false;
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
    void DrawAreaOverlays()
    {
        if (!world_map_context) return;
        if (world_map_context->zoom != 1.f && world_map_context->zoom != .0f) return; // Map is animating

        for (const auto& [file_id, info] : map_info_by_file_id) {
            // Filter to the continent currently shown on the world map.
            const auto map_info = GW::Map::GetMapInfo(info.map_id);
            if (!(map_info && map_info->continent == world_map_context->continent)) continue;

            const ImVec2 screen_min = CalculateViewportPos(info.world_pos_start, world_map_context->top_left);
            const ImVec2 screen_max = CalculateViewportPos(info.world_pos_end, world_map_context->top_left);

            draw_list->AddRectFilled(screen_min, screen_max, IM_COL32(0, 153, 0, 64));
            draw_list->AddRect(screen_min, screen_max, IM_COL32(0, 200, 0, 128));
        }
    }

    bool DrawPortalOnWorldMap(const MapPortal& portal)
    {
        if (!world_map_context) return false;
        if (world_map_context->zoom != 1.f && world_map_context->zoom != .0f) return false; // Map is animating

        auto& pos = portal.world_pos;
        const auto viewport_pos = CalculateViewportPos(pos, world_map_context->top_left);
        const ImRect icon_rect = {{viewport_pos.x - quest_icon_size_half, viewport_pos.y - quest_icon_size_half}, {viewport_pos.x + quest_icon_size_half, viewport_pos.y + quest_icon_size_half}};

        draw_list->AddImage(*quest_icon_texture, icon_rect.Min, icon_rect.Max);

        return icon_rect.Contains(ImGui::GetMousePos());
    }
} // namespace

GW::Constants::MapID WorldMapWidget::GetMapIdForLocation(const GW::Vec2f& world_map_pos, GW::Constants::MapID exclude_map_id)
{
    auto map_id = GW::Map::GetMapID();
    auto map_info = GW::Map::GetMapInfo();
    if (!map_info) return GW::Constants::MapID::None;
    const auto continent = map_info->continent;
    if (map_id != exclude_map_id && MapContainsWorldPos(map_id, world_map_pos, continent)) return map_id;
    for (size_t i = 1; i < static_cast<size_t>(GW::Constants::MapID::Count); i++) {
        map_id = static_cast<GW::Constants::MapID>(i);
        if (map_id == exclude_map_id) continue;
        map_info = GW::Map::GetMapInfo(map_id);
        if (!(map_info && map_info->GetIsOnWorldMap())) continue;
        if (MapContainsWorldPos(map_id, world_map_pos, continent)) return map_id;
    }
    return GW::Constants::MapID::None;
}

void WorldMapWidget::Initialize()
{
    ToolboxWidget::Initialize();

    memset(show_elite_capture_locations, true, sizeof(show_elite_capture_locations));
    quest_icon_texture = GwDatTextureModule::LoadTextureFromFileId(0x1b4d5);
    player_icon_texture = GwDatTextureModule::LoadTextureFromFileId(0x5d3b);
    portal_icon_texture = GwDatTextureModule::LoadTextureFromFileId(0x246c); // IDirect3DTexture9**

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

    const GW::UI::UIMessage ui_messages[] = {GW::UI::UIMessage::kQuestAdded,      GW::UI::UIMessage::kSendSetActiveQuest, GW::UI::UIMessage::kMapLoaded,
                                             GW::UI::UIMessage::kOnScreenMessage, GW::UI::UIMessage::kSendAbandonQuest,   GW::UI::UIMessage::kLoadMapContext};
    for (auto ui_message : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&OnUIMessage_HookEntry, ui_message, OnUIMessage, 0x8000);
    }

    AppendMapFileInfo();
}

bool WorldMapWidget::WorldMapToGamePos(const GW::Vec2f& world_map_pos, GW::GamePos& game_map_pos)
{
    ImRect map_bounds;
    if (!GW::Map::GetMapWorldMapBounds(GW::Map::GetMapInfo(), &map_bounds)) return false;

    const auto current_map_context = GW::GetMapContext();
    if (!current_map_context) return false;

    const auto game_map_rect = ImRect({current_map_context->start_pos.x, current_map_context->start_pos.y, current_map_context->end_pos.x, current_map_context->end_pos.y});

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
    if (game_map_pos.x == INFINITY || game_map_pos.y == INFINITY) return false;
    ImRect map_bounds;
    if (!GW::Map::GetMapWorldMapBounds(GW::Map::GetMapInfo(), &map_bounds)) return false;
    const auto current_map_context = GW::GetMapContext();
    if (!current_map_context) return false;

    const auto game_map_rect = ImRect({current_map_context->start_pos.x, current_map_context->start_pos.y, current_map_context->end_pos.x, current_map_context->end_pos.y});

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
    if (view_all_outposts_patch.IsValid()) view_all_outposts_patch.TogglePatch(show);
    if (view_all_carto_areas_patch.IsValid()) view_all_carto_areas_patch.TogglePatch(show);
    TriggerWorldMapRedraw();
}

void WorldMapWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    LOAD_BOOL(showing_all_outposts);
    LOAD_BOOL(show_lines_on_world_map);
    LOAD_BOOL(showing_all_quests);
    LOAD_BOOL(apply_quest_colors);
    LOAD_BOOL(hide_captured_elites);
    LOAD_BOOL(show_any_elite_capture_locations);
    LOAD_BOOL(hide_captured_elites);
    uint32_t show_elite_capture_locations_val = 0xffffffff;
    LOAD_UINT(show_elite_capture_locations_val);
    for (size_t i = 0; i < _countof(show_elite_capture_locations); i++) {
        show_elite_capture_locations[i] = ((show_elite_capture_locations_val >> i) & 0x1) != 0;
    }
    ShowAllOutposts(showing_all_outposts);


    const std::filesystem::path map_info_by_file_id_file = Resources::GetPath(L"MapInfoByFileId.txt");
    std::ifstream in(map_info_by_file_id_file);
    if (in.is_open()) {
        std::string line;
        while (std::getline(in, line)) {
            if (line.substr(0, 4) == "MAP ") {
                std::istringstream ss(line.substr(4));
                MapFileInfo info;
                size_t portal_count = 0;
                uint32_t map_id = 0;
                uint32_t continent = 0;
                if (!(ss >> continent >> info.map_file_id >> info.world_pos_start.x >> info.world_pos_start.y >> info.world_pos_end.x >> info.world_pos_end.y >> portal_count >> map_id)) continue;
                info.map_id = static_cast<GW::Constants::MapID>(map_id);
                info.continent = static_cast<GW::Continent>(continent);
                info.portals.reserve(portal_count);
                for (size_t i = 0; i < portal_count && std::getline(in, line); i++) {
                    if (line.substr(0, 7) != "PORTAL ") {
                        --i;
                        continue;
                    }
                    std::istringstream pss(line.substr(7));
                    MapPortal portal;
                    if (!(pss >> portal.map_file_id >> portal.prop_index >> portal.world_pos.x >> portal.world_pos.y)) continue;
                    info.portals.push_back(portal);
                }
                map_info_by_file_id[info.map_file_id] = std::move(info);
            }
        }
        // Linked portals span across maps, so resolve them in a second pass
        // once the entire file has been loaded.
        for (auto& [_, info] : map_info_by_file_id) {
            for (auto& portal : info.portals) {
                portal.checkForLinkedPortal(info.continent);
            }
        }
    }
}

void WorldMapWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_BOOL(showing_all_outposts);
    SAVE_BOOL(show_lines_on_world_map);
    SAVE_BOOL(showing_all_quests);
    SAVE_BOOL(apply_quest_colors);
    SAVE_BOOL(show_any_elite_capture_locations);
    SAVE_BOOL(hide_captured_elites);
    uint32_t show_elite_capture_locations_val = 0;
    for (size_t i = 0; i < _countof(show_elite_capture_locations); i++) {
        if (show_elite_capture_locations[i]) {
            show_elite_capture_locations_val |= (1u << i);
        }
    }
    SAVE_UINT(show_elite_capture_locations_val);

    const std::filesystem::path map_info_by_file_id_file = Resources::GetPath(L"MapInfoByFileId.txt");
    // File format (plain text, one map block per entry):
    //   MAP <map_file_id> <world_pos_start.x> <world_pos_start.y> <world_pos_end.x> <world_pos_end.y> <portal_count> <map_id>
    //   PORTAL <map_file_id> <prop_model_file_id> <world_pos.x> <world_pos.y>
    //   ...
    std::ofstream out(map_info_by_file_id_file);
    if (!out.is_open()) return;
    for (const auto& [file_id, info] : map_info_by_file_id) {
        out << "MAP " << static_cast<uint32_t>(info.map_id)  << info.map_file_id << " " << info.world_pos_start.x << " " << info.world_pos_start.y << " " << info.world_pos_end.x << " " << info.world_pos_end.y << " " << info.portals.size() << " " << static_cast<uint32_t>(info.map_id) << "\n";
        for (const auto& portal : info.portals) {
            out << "PORTAL " << portal.map_file_id << " " << portal.prop_index << " " << portal.world_pos.x << " " << portal.world_pos.y << "\n";
        }
    }
}

void WorldMapWidget::Draw(IDirect3DDevice9*)
{
    if (!(GW::UI::GetIsWorldMapShowing() && PreCalculateFrameVars())) {
        // ShowAllOutposts(showing_all_outposts = false);
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
    ImGui::Checkbox("Show elite capture locations", &show_any_elite_capture_locations);
    if (show_any_elite_capture_locations) {
        ImGui::Indent();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0.f, 0.f});
        for (size_t i = 1; i < _countof(show_elite_capture_locations); i++) {
            const auto icon = Resources::GetProfessionIcon((GW::Constants::Profession)i);
            if (!(icon && *icon)) continue;
            if (i != 1) ImGui::SameLine();
            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button, show_elite_capture_locations[i] ? completed_bg.Value : ImGui::GetStyleColorVec4(ImGuiCol_Button));
            if (ImGui::IconButton("", *icon, {24.f, 24.f})) {
                show_elite_capture_locations[i] = !show_elite_capture_locations[i];
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
        ImGui::PopStyleVar();
        ImGui::Checkbox("Hide elites already captured", &hide_captured_elites);
        if (hide_captured_elites) {
            const auto& completion = CompletionWindow::Instance().GetCharacterCompletion(GW::PlayerMgr::GetPlayerName(), false);
            if (!completion) ImGui::TextDisabled("Limited to your primary/secondary profession if Completion Window is disabled");
        }
        ImGui::Unindent();
    }
    // ImGui::InputFloat("region.x", &tarnished_coast.x, 10.f);
    // ImGui::InputFloat("region.y", &tarnished_coast.y, 10.f);
    // ImGui::InputFloat("default_scale", &default_scale, 0.001f);
    ImGui::End();
    ImGui::PopStyleColor();


    if (window) {
        controls_window_rect = window->Rect();
        controls_window_rect.Translate(mouse_offset);
    }
    hovered_map_portal = 0;
    #ifdef _DEBUG
    DrawAreaOverlays();
    const auto current_map_info = GW::Map::GetMapInfo();
    for (auto& [_, map_info] : map_info_by_file_id) {
        if (!(current_map_info && map_info.continent == current_map_info->continent)) continue;
        for (auto& portal : map_info.portals) {
            if (DrawPortalOnWorldMap(portal)) {
                hovered_map_portal = &portal;
            }
        }
    }
    #endif


    hovered_boss = nullptr;
    locations_assigned_to_outposts.clear();
    for (auto& boss : elite_boss_locations) {
        if (DrawBossLocationOnWorldMap(boss)) {
            hovered_boss = &boss;
        }
    }

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
            if (!quest_name.IsDecoding()) quest_name.reset(hovered_quest->name);
            ImGui::SetTooltip("%s", quest_name.string().c_str());
        }
    }
    if (hovered_boss) {
        ImGui::SetTooltip("%s", BossInfo(hovered_boss).c_str());
    }
    if (hovered_map_portal) {
        ImGui::SetTooltip([]() {
            if (hovered_map_portal) DrawMapPortalInfo(hovered_map_portal);
        });
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
        for (auto& line : lines | std::views::filter([](auto line) {
                              return line->visible;
                          })) {
            if (line->map != map_id) continue;
            if (!GamePosToWorldMap(line->p1, line_start)) continue;
            if (!GamePosToWorldMap(line->p2, line_end)) continue;

            line_start.x = (line_start.x - world_map_context->top_left.x) * ui_scale.x + viewport_offset.x;
            line_start.y = (line_start.y - world_map_context->top_left.y) * ui_scale.y + viewport_offset.y;
            line_end.x = (line_end.x - world_map_context->top_left.x) * ui_scale.x + viewport_offset.x;
            line_end.y = (line_end.y - world_map_context->top_left.y) * ui_scale.y + viewport_offset.y;

            draw_list->AddLine(line_start, line_end, line->color);
        }
    }
    if (show_any_elite_capture_locations) {
        const auto rect = draw_list->GetClipRectMax();
        const auto text = "Elite capture locations extracted from MappingOut v4.0.0 by Aylee Sedai";
        draw_list->AddText({16.f, rect.y - 28.f}, ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
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
            if (!(world_map_context && GW::UI::GetIsWorldMapShowing())) break;
            if (GW::QuestMgr::GetQuest(hovered_quest_id)) {
                ImGui::SetContextMenu(HoveredQuestContextMenu, (void*)hovered_quest_id);
                break;
            }

            if (!(world_map_context && world_map_context->zoom == 1.0f)) break;
            world_map_click_pos = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            world_map_click_pos.x /= ui_scale.x;
            world_map_click_pos.y /= ui_scale.y;
            world_map_click_pos.x += world_map_context->top_left.x;
            world_map_click_pos.y += world_map_context->top_left.y;
            if (hovered_boss) {
                ImGui::SetContextMenu(EliteBossLocationContextMenu, (void*)hovered_boss);
                break;
            }
            if (hovered_map_portal) {
                ImGui::SetContextMenu(MapPortalContextMenu, (void*)hovered_map_portal);
                break;
            }
            ImGui::SetContextMenu(WorldMapContextMenu);
        } break;
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_LBUTTONDOWN:
            if (!drawn || !GW::UI::GetIsWorldMapShowing()) return false;
            if (ImGui::ShowingContextMenu()) return true;
            if (check_rect(controls_window_rect)) return true;
            break;
    }
    return false;
}

void WorldMapWidget::DrawSettingsInternal()
{
    ImGui::Text("Note: only visible in Hard Mode explorable areas.");
}
