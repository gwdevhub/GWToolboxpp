#include "stdafx.h"

#include <fstream>
#include <sstream>

#include <GWCA/Constants/Maps.h>

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

#include <Widgets/CartographerWidget.h>
#include <Modules/GwDatModule.h>
#include <Modules/Resources.h>
#include <Widgets/Minimap/AgentRenderer.h>
#include <Widgets/Minimap/Minimap.h>
#include <Widgets/Minimap/GameWorldRenderer.h>

#include <Widgets/WorldMapWidget.h>
#include <Widgets/WorldMapWidget_Constants.h>

#include <Windows/CompletionWindow.h>
#include <Windows/DailyQuestsWindow.h>
#include <Windows/TravelWindow.h>

#include <Utils/GuiUtils.h>
#include <Utils/ToolboxUtils.h>

#include "Defines.h"

#include <Color.h>
#include <GWCA/Managers/AgentMgr.h>
#include <ImGuiAddons.h>
#include <Modules/QuestModule.h>
#include <Utils/ArenaNetFileParser.h>
#include <Utils/TextUtils.h>
#include <Windows/Pathfinding/PathfindingWindow.h>
#include <Windows/Pathfinding/PathingMapDataLoader.h>
#include <corecrt_math_defines.h>




namespace {
    using namespace WorldMapWidget_Constants;

    struct MapPortal;

    uint32_t current_map_file_id = 0;
    GW::Constants::MapID current_map_id = GW::Constants::MapID::None;

    struct MapFileInfo {
        GW::Continent continent;
        GW::Vec2f world_pos_start; // 边界左上角
        GW::Vec2f world_pos_end;   // 边界右下角
        uint32_t map_file_id;      // 此地图的唯一标识符
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
    IDirect3DTexture9** zaishen_coin_texture = nullptr;

    WorldMapWidget::Settings settings;

    bool show_elite_capture_locations[11];
    bool show_elite_capture_locations_campaign[4]; // 核心=0, 预言=1, 派系=2, 夜幕=3
    bool drawn = false;

    GW::MemoryPatcher view_all_outposts_patch;
    GW::MemoryPatcher view_all_carto_areas_patch;

    bool world_map_clicking = false;
    GW::Vec2f world_map_click_pos;
    bool world_map_click_pos_valid = false;

    GW::Constants::QuestID hovered_quest_id = GW::Constants::QuestID::None;
    GuiUtils::EncString hovered_quest_name;
    GuiUtils::EncString hovered_quest_description;
    const EliteBossLocation* hovered_boss = nullptr;
    const MapPortal* hovered_map_portal = nullptr;

    // 每帧更新的缓存变量；避免在 DrawQuestMarkerOnWorldMap 中重复计算
    GW::Vec2f player_world_map_pos;
    float player_rotation = .0f;
    GW::Vec2f viewport_offset;
    GW::Vec2f ui_scale;
    float world_map_scale = 1.f;
    GW::Vec2f world_map_proj_scale = {1.f, 1.f}; // 每世界地图坐标的像素，考虑动画
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
                mission_suffix = "（任务）";
                break;
            case GW::RegionType::Challenge:
                mission_suffix = "（挑战）";
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

        ImGui::Text("属性索引：%d", portal->prop_index);
        ImGui::Text("地图文件 ID：%d", portal->map_file_id);
        if (include_linked) {
            if (const auto linked = portal->linkedPortal()) {
                ImGui::Text("连接到：");
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

    std::vector<WorldMapWidget::ContextMenuCallback> context_menu_callbacks;
    std::vector<WorldMapWidget::OverlayCallback> overlay_callbacks;

    bool ContextMenuMarkerButtons()
    {
        if (ImGui::Button("放置标记")) {
            GW::GameThread::Enqueue([] {
                QuestModule::SetCustomQuestMarker(world_map_click_pos, true);
            });
            return false;
        }
        if (QuestModule::GetCustomQuestMarker()) {
            if (ImGui::Button("移除标记")) {
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
        for (const auto& cb : context_menu_callbacks) {
            if (!cb()) return false;
        }
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
        const auto size = ImVec2(250.0f * ImGui::FontScale(), 0);
        ImGui::Separator();
        const bool set_active = ImGui::Button("设为激活任务", size);
        const bool travel = ImGui::Button("前往最近的前哨站", size);
        const bool wiki = ImGui::Button("激战维基", size);

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::Separator();
        if (!ContextMenuMarkerButtons()) return false;
        if (world_map_click_pos_valid) {
            for (const auto& cb : context_menu_callbacks) {
                if (!cb()) return false;
            }
        }

        if (set_active) {
            GW::GameThread::Enqueue([quest_id] {
                QuestModule::SetActiveQuestId(quest_id);
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
        const auto size = ImVec2(250.0f * ImGui::FontScale(), 0);
        ImGui::Separator();

        const bool travel = ImGui::Button("前往最近的前哨站", size);

        const auto boss_label = std::format("在激战维基上查看 {}", boss->boss_name);
        const bool boss_wiki = ImGui::Button(boss_label.c_str(), size);

        const auto skill_label = std::format("在激战维基上查看 {}", Resources::GetSkillName(boss->skill_id)->string());
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
        if (!(prop && prop->model_info)) return 0;
        return ArenaNetFileParser::FileHashToFileId(prop->model_info->model_file_name);
    };

    bool IsTravelPortal(GW::MapProp* prop)
    {
        switch (GetMapPropModelFileId(prop)) {
            case 0x4e6b2: // Eotn 阿苏拉传送门
            case 0x3c5ac: // Eotn，夜幕
            case 0xa825:  // 预言，派系
                return true;
        }
        return false;
    }

    bool IsValidOutpost(GW::Constants::MapID map_id)
    {
        const auto map_info = GW::Map::GetMapInfo(map_id);
        if (!GW::Map::HasMapDisplayInfo(map_info) || GW::Map::IsExcludedMapInfo(map_info)) return false;
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

    bool IsHighlightableLockedArea(GW::Constants::MapID map_id, const GW::AreaInfo* map_info)
    {
        if (!(map_info && map_info->GetIsOnWorldMap())) return false;
        if (GW::Map::IsPreSearing(map_id) != GW::Map::IsPreSearing() || GW::Map::IsFestivalOutpost(map_id)) return false;
        if (GW::Map::IsExcludedMapInfo(map_info)) return false;
        switch (map_info->type) {
            case GW::RegionType::City:
            case GW::RegionType::CooperativeMission:
            case GW::RegionType::EliteMission:
            case GW::RegionType::MissionOutpost:
            case GW::RegionType::Outpost:
                return map_id != GW::Constants::MapID::Gate_of_Anguish_elite_mission;
            default:
                return false;
        }
    }

    GW::Constants::MapID GetClosestMapToPoint(const GW::Vec2f& world_map_point)
    {
        for (size_t i = 0; i < (size_t)GW::Constants::MapID::Count; i++) {
            const auto map_info = GW::Map::GetMapInfo((GW::Constants::MapID)i);
            if (!GW::Map::HasMapDisplayInfo(map_info) || GW::Map::IsExcludedMapInfo(map_info)) continue;
            if (!map_info->GetIsOnWorldMap()) continue;
            (world_map_point);
            // TODO：点到矩形的距离
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
            float distance = GW::GetDistance(prop->position, game_pos);

            if (!found || distance < closest_distance) {
                found = prop;
                closest_distance = distance;
            }
        }
        return found;
    }

    void AppendMapFileInfo()
    {
        if (!current_map_file_id || map_info_by_file_id.contains(current_map_file_id)) return;
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

    void CalculateRotatedPoints(const ImRect& rect, const ImVec2& center, float rotation_angle, ImVec2 out_points[4])
    {
        ImVec2 points[4] = {
            rect.Min,                 // 左上
            {rect.Max.x, rect.Min.y}, // 右上
            rect.Max,                 // 右下
            {rect.Min.x, rect.Max.y}  // 左下
        };

        for (int i = 0; i < 4; ++i) {
            const float dx = points[i].x - center.x;
            const float dy = points[i].y - center.y;

            out_points[i] = {center.x + dx * cos(rotation_angle) - dy * sin(rotation_angle), center.y + dx * sin(rotation_angle) + dy * cos(rotation_angle)};
        }
    }

    void CalculateUVCoords(float uv_start_x, float uv_end_x, ImVec2 uv_points[4])
    {
        uv_points[0] = {uv_start_x, 0.0f}; // 左上
        uv_points[1] = {uv_end_x, 0.0f};   // 右上
        uv_points[2] = {uv_end_x, 1.0f};   // 右下
        uv_points[3] = {uv_start_x, 1.0f}; // 左下
    }



    ImVec2 CalculateViewportPos(const GW::Vec2f& marker_world_pos, const ImVec2& top_left)
    {
        return {world_map_proj_scale.x * (marker_world_pos.x - top_left.x) + viewport_offset.x, world_map_proj_scale.y * (marker_world_pos.y - top_left.y) + viewport_offset.y};
    }

    GW::Vec2f GetMapMarkerPoint(GW::AreaInfo* map_info)
    {
        if (!map_info) return {};
        if (map_info->x && map_info->y) {
            // 如果地图有图标 x 和 y 坐标，将其用作自定义任务标记位置
            // 注意：GW 将此标记放在前哨站图标的顶部，而非中心 — 可能是为了更容易看到？听起来很蠢，不要模仿。
            return {(float)map_info->x, (float)map_info->y};
        }
        if (map_info->icon_start_x && map_info->icon_start_y) {
            // 否则使用地图名称标签的中心位置
            return {(float)(map_info->icon_start_x + ((map_info->icon_end_x - map_info->icon_start_x) / 2)), (float)(map_info->icon_start_y + ((map_info->icon_end_y - map_info->icon_start_y) / 2))};
        }
        // 否则使用地图名称标签的中心位置
        return {(float)(map_info->icon_start_x_dupe + ((map_info->icon_end_x_dupe - map_info->icon_start_x_dupe) / 2)), (float)(map_info->icon_start_y_dupe + ((map_info->icon_end_y_dupe - map_info->icon_start_y_dupe) / 2))};
    }

    // 预计算此帧的一些缓存变量，避免重复计算
    bool PreCalculateFrameVars()
    {
        world_map_context = GW::Map::GetWorldMapContext();
        if (!world_map_context) return false;
        const auto viewport = ImGui::GetMainViewport();
        viewport_offset = viewport->Pos;
        draw_list = ImGui::GetBackgroundDrawList(viewport);
        const auto world_map_frame = GW::UI::GetFrameById(world_map_context->frame_id);
        ui_scale = world_map_frame->position.GetViewportScale(GW::UI::GetRootFrame());

        const auto me = GW::Agents::GetControlledCharacter();
        if (!(me && WorldMapWidget::GamePosToWorldMap(me->pos, player_world_map_pos))) return false;
        player_rotation = me->rotation_angle;

        const GW::Vec2f world_map_size_in_coords = {(float)world_map_context->h004c[5], (float)world_map_context->h004c[6]};
        const GW::Vec2f world_map_zoomed_out_size = {world_map_context->h0030, world_map_context->h0034};

        world_map_scale = 1.f;
        if (world_map_context->zoom != 1.0f) {
            // 如果我们缩放了，世界地图坐标不是 1:1 比例；我们需要找到比例因子
            if (world_map_context->top_left.y == 0.f) {
                // 缩放的地图垂直填充
                world_map_scale = world_map_zoomed_out_size.y / world_map_size_in_coords.y;
            }
            else {
                // 缩放的地图水平填充
                world_map_scale = world_map_zoomed_out_size.x / world_map_size_in_coords.x;
            }
        }
        world_map_proj_scale = {ui_scale.x * world_map_scale, ui_scale.y * world_map_scale};
        if (world_map_frame) {
            const auto frame_size = world_map_frame->position.GetSizeOnScreen();
            const auto span = world_map_context->bottom_right - world_map_context->top_left;
            if (span.x != 0.f && span.y != 0.f && frame_size.x > 0.f && frame_size.y > 0.f) {
                world_map_proj_scale = {frame_size.x / span.x, frame_size.y / span.y};
            }
        }

        quest_icon_size = 24.0f * ui_scale.x;
        quest_icon_size_half = quest_icon_size / 2.f;

        constexpr float FULL_ROTATION_TIME = 16.0f;
        const float elapsed_seconds = static_cast<float>(TIMER_INIT()) / CLOCKS_PER_SEC;
        quest_star_rotation_angle = 2.0f * (float)M_PI * fmod(elapsed_seconds, FULL_ROTATION_TIME) / FULL_ROTATION_TIME;

        return true;
    }

    ImVec2 skill_texture_size = {};

    std::unordered_map<GW::Constants::MapID, uint32_t> locations_assigned_to_outposts;

    bool DrawBossLocationOnWorldMap(const EliteBossLocation& boss)
    {
        if (!settings.show_any_elite_capture_locations) return false;
        if (!(world_map_context)) return false;

        const auto map_info = GW::Map::GetMapInfo(boss.map_id);
        if (!(map_info && map_info->continent == world_map_context->continent)) return false;

        const auto skill = GW::SkillbarMgr::GetSkillConstantData(boss.skill_id);
        if (!skill) return false;
        if (!show_elite_capture_locations[(uint32_t)skill->profession]) return false;
        const auto campaign_idx = (uint32_t)skill->campaign;
        if (campaign_idx < _countof(show_elite_capture_locations_campaign) && !show_elite_capture_locations_campaign[campaign_idx]) return false;
        if (settings.hide_captured_elites) {
            const auto me = GW::Agents::GetControlledCharacter();
            if (me->primary == skill->profession || me->secondary == skill->profession) {
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

        if (!(texture && *texture)) return false;

        if (!Resources::GetTextureSize(*texture, &skill_texture_size)) return false;

        const float icon_size = std::lerp(16.f, 32.f, std::clamp(world_map_context->zoom, 0.f, 1.f)); // 随缩放增长
        const auto half_size = icon_size / 2.f;

        const auto prof_idx = static_cast<uint32_t>(skill->profession);
        const auto prof_color = (settings.color_elite_icons_by_profession && prof_idx)
            ? AgentRenderer::Instance().GetProfessionColor(prof_idx)
            : 0u;

        bool hovered = false;
        const auto draw_boss_icon = [&](const GW::Vec2f& boss_pos) {
            const auto viewport_boss_pos = CalculateViewportPos(boss_pos, world_map_context->top_left);
            const ImRect icon_rect = {{viewport_boss_pos.x - half_size, viewport_boss_pos.y - half_size}, {viewport_boss_pos.x + half_size, viewport_boss_pos.y + half_size}};
            ImGui::AddImageScaled(draw_list, *texture, icon_rect.Min, skill_texture_size, icon_size, icon_size);
            if (prof_color) {
                draw_list->AddRect(icon_rect.Min, icon_rect.Max, prof_color, 0.f, 0, 2.f);
            }
            hovered |= icon_rect.Contains(ImGui::GetMousePos());
        };
        if (boss.coords.empty()) {
            // Dungeons and the Realm of Torment have no area on the world map, so stack their icons next to the entrance outpost
            const auto slot = locations_assigned_to_outposts[boss.map_id]++;
            draw_boss_icon({(float)map_info->x - icon_size * 2.f + icon_size * slot, (float)map_info->y});
        }
        else {
            for (const auto& boss_pos : boss.coords) {
                draw_boss_icon(boss_pos);
            }
        }

        return hovered;
    }

    bool DrawQuestMarkerOnWorldMap(const GW::Quest* quest)
    {
        if (!(world_map_context && quest)) return false;
        if (!(quest_icon_texture && *quest_icon_texture)) return false;


        bool is_hovered = false;
        auto color = GW::QuestMgr::GetActiveQuestId() == quest->quest_id ? 0 : 0x80FFFFFF;
        if (settings.apply_quest_colors) {
            color = QuestModule::GetQuestColor(quest->quest_id);
        }

        const auto draw_quest_marker = [&](const GW::Vec2f& quest_marker_pos) {
            const auto viewport_quest_pos = CalculateViewportPos(quest_marker_pos, world_map_context->top_left);

            const ImRect icon_rect = {{viewport_quest_pos.x - quest_icon_size_half, viewport_quest_pos.y - quest_icon_size_half}, {viewport_quest_pos.x + quest_icon_size_half, viewport_quest_pos.y + quest_icon_size_half}};

            ImVec2 rotated_points[4];
            CalculateRotatedPoints(icon_rect, viewport_quest_pos, quest_star_rotation_angle, rotated_points);

            ImVec2 uv_points[4];
            CalculateUVCoords(0.0f, 0.5f, uv_points); // 精灵地图左侧

            draw_list->AddImageQuad(*quest_icon_texture, rotated_points[0], rotated_points[1], rotated_points[2], rotated_points[3], uv_points[0], uv_points[1], uv_points[2], uv_points[3], color & IM_COL32_A_MASK ? color : IM_COL32_WHITE);

            if (zaishen_coin_texture && *zaishen_coin_texture && DailyQuests::GetZaishenCoinReward(quest->quest_id)) {
                const float coin_half = quest_icon_size * 0.3f;
                draw_list->AddImage(*zaishen_coin_texture, {viewport_quest_pos.x - coin_half, viewport_quest_pos.y - coin_half}, {viewport_quest_pos.x + coin_half, viewport_quest_pos.y + coin_half});
            }

            return icon_rect.Contains(ImGui::GetMousePos());
        };

        const auto draw_quest_arrow = [&](const GW::Vec2f& quest_marker_pos) {
            const auto viewport_quest_pos = CalculateViewportPos(quest_marker_pos, world_map_context->top_left);
            const auto viewport_player_pos = CalculateViewportPos(player_world_map_pos, world_map_context->top_left);
            const float dx = viewport_quest_pos.x - viewport_player_pos.x;
            const float dy = viewport_quest_pos.y - viewport_player_pos.y;

            // 使用 atan2 计算旋转角度（弧度），指向远离玩家的方向
            float rotation_angle = std::atan2f(-dy, -dx);
            rotation_angle += DirectX::XM_PI;

            const ImRect icon_rect = {{viewport_quest_pos.x - quest_icon_size_half, viewport_quest_pos.y - quest_icon_size_half}, {viewport_quest_pos.x + quest_icon_size_half, viewport_quest_pos.y + quest_icon_size_half}};

            ImVec2 rotated_points[4];
            CalculateRotatedPoints(icon_rect, viewport_quest_pos, rotation_angle, rotated_points);

            ImVec2 uv_points[4];
            CalculateUVCoords(0.5f, 1.0f, uv_points); // 精灵地图右侧

            draw_list->AddImageQuad(*quest_icon_texture, rotated_points[0], rotated_points[1], rotated_points[2], rotated_points[3], uv_points[0], uv_points[1], uv_points[2], uv_points[3], color & IM_COL32_A_MASK ? color : IM_COL32_WHITE);

            return icon_rect.Contains(ImGui::GetMousePos());
        };

        // 任务不在此地图结束；标记图标需要是箭头，实际标记需要定位到目标地图的标签上
        const auto map_info = GW::Map::GetMapInfo(quest->map_to);
        if (!(map_info && map_info->continent == world_map_context->continent)) return false;
        GW::Vec2f pos;
        if (QuestModule::GetCustomQuestMarkerWorldPos(quest->quest_id, pos)) {
            return draw_quest_marker(pos);
        }

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

        for (const auto& [file_id, info] : map_info_by_file_id) {
            // 过滤到当前在世界地图上显示的大陆。
            const auto map_info = GW::Map::GetMapInfo(info.map_id);
            if (!(map_info && map_info->continent == world_map_context->continent)) continue;

            const ImVec2 screen_min = CalculateViewportPos(info.world_pos_start, world_map_context->top_left);
            const ImVec2 screen_max = CalculateViewportPos(info.world_pos_end, world_map_context->top_left);

            draw_list->AddRectFilled(screen_min, screen_max, IM_COL32(0, 153, 0, 64));
            draw_list->AddRect(screen_min, screen_max, IM_COL32(0, 200, 0, 128));
        }
    }

    void DrawLockedAreaHighlights()
    {
        if (!(settings.showing_all_outposts && settings.highlight_locked_areas && world_map_context)) return;
        if (!Colors::IsVisible(settings.locked_area_highlight_color)) return;

        std::unordered_set<uint32_t> highlighted_names;
        for (size_t i = 1; i < static_cast<size_t>(GW::Constants::MapID::Count); i++) {
            const auto map_id = static_cast<GW::Constants::MapID>(i);
            const auto map_info = GW::Map::GetMapInfo(map_id);
            if (!(IsHighlightableLockedArea(map_id, map_info) && map_info->continent == world_map_context->continent)) continue;
            if (map_info->name_id && highlighted_names.contains(map_info->name_id)) continue;
            if (GW::Map::GetIsMapUnlocked(map_id)) continue;

            if (map_info->name_id) {
                highlighted_names.insert(map_info->name_id);
            }

            const auto marker_pos = CalculateViewportPos(GetMapMarkerPoint(map_info), world_map_context->top_left);
            const auto radius = 8.f * ui_scale.x;
            draw_list->AddCircleFilled(marker_pos, radius, settings.locked_area_highlight_color);
            draw_list->AddCircle(marker_pos, radius, Colors::FullAlpha(settings.locked_area_highlight_color));
        }
    }

    bool DrawPortalOnWorldMap(const MapPortal& portal)
    {
        if (!world_map_context) return false;
        if (!(quest_icon_texture && *quest_icon_texture)) return false;

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
    // These are all drawn by the controls window on the world map itself, not in the Settings window,
    // so keep them out of settings search - a result here would navigate to a section without them.
    SettingsRegistry::Register(this, settings, false);

    memset(show_elite_capture_locations, true, sizeof(show_elite_capture_locations));
    quest_icon_texture = GwDatModule::LoadTextureFromFileId(0x1b4d5);
    player_icon_texture = GwDatModule::LoadTextureFromFileId(0x5d3b);
    portal_icon_texture = GwDatModule::LoadTextureFromFileId(0x246c); // IDirect3DTexture9**
    zaishen_coin_texture = GwDatModule::LoadTextureFromFileId(0x55778);

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
        RegisterUIMessageCallback(&OnUIMessage_HookEntry, ui_message, OnUIMessage, 0x8000);
    }

    AppendMapFileInfo();
}

namespace {
    // 世界地图每单位 96 gwinches，硬编码在 GW 源代码中。
    constexpr float gwinches_per_unit = 96.f;

    // `map_id` 的世界地图中点（来自缓存 DAT 的游戏边界）— 两种转换共享的锚点。
    bool GetMapWorldAnchor(GW::Constants::MapID map_id, GW::Vec2f& mid_out)
    {
        if ((uint32_t)map_id == 0) map_id = GW::Map::GetMapID();

        GW::Vec2f game_min, game_max;
        if (map_id == GW::Map::GetMapID()) {
            const auto map_context = GW::GetMapContext();
            if (!map_context) return false;
            game_min = {map_context->start_pos.x, map_context->start_pos.y};
            game_max = {map_context->end_pos.x, map_context->end_pos.y};
        }
        else {
            Pathing::Vec2f bmin, bmax;
            if (!Pathing::GetMapGameBoundsFromDAT(PathfindingWindow::GetMapFileId(map_id), bmin, bmax)) return false;
            game_min = {bmin.x, bmin.y};
            game_max = {bmax.x, bmax.y};
        }

        const auto area_info = GW::Map::GetMapInfo(map_id);
        ImRect map_bounds;
        if (!area_info || !GW::Map::GetMapWorldMapBounds(area_info, &map_bounds)) return false;

        mid_out = {
            map_bounds.Min.x - (game_min.x / gwinches_per_unit),
            map_bounds.Min.y + (game_max.y / gwinches_per_unit) + 1.f,
        };
        return true;
    }
} // namespace

bool WorldMapWidget::WorldMapToGamePos(const GW::Vec2f& world_map_pos, GW::GamePos& game_map_pos, GW::Constants::MapID map_id)
{
    GW::Vec2f mid;
    if (!GetMapWorldAnchor(map_id, mid)) return false;

    game_map_pos.x = (world_map_pos.x - mid.x) * gwinches_per_unit;
    game_map_pos.y = (world_map_pos.y - mid.y) * gwinches_per_unit * -1.f; // 反转 Y 轴
    return true;
}

bool WorldMapWidget::GamePosToWorldMap(const GW::GamePos& game_map_pos, GW::Vec2f& world_map_pos, GW::Constants::MapID map_id)
{
    if (game_map_pos.x == INFINITY || game_map_pos.y == INFINITY) return false;
    GW::Vec2f mid;
    if (!GetMapWorldAnchor(map_id, mid)) return false;

    world_map_pos.x = (game_map_pos.x / gwinches_per_unit) + mid.x;
    world_map_pos.y = ((game_map_pos.y * -1.f) / gwinches_per_unit) + mid.y; // 反转 Y 轴
    return true;
}

bool WorldMapWidget::GetMapMarkerWorldPos(GW::Constants::MapID map_id, GW::Vec2f& out)
{
    const auto map_info = GW::Map::GetMapInfo(map_id);
    if (!map_info) return false;
    out = GetMapMarkerPoint(map_info);
    return out.x != 0 || out.y != 0;
}

void WorldMapWidget::SignalTerminate()
{
    ToolboxWidget::Terminate();

    view_all_outposts_patch.Reset();
    view_all_carto_areas_patch.Reset();
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_HookEntry);
}

bool& WorldMapWidget::ShowLinesOnWorldMap()
{
    return settings.show_lines_on_world_map;
}

void WorldMapWidget::AddContextMenuCallback(ContextMenuCallback cb) { context_menu_callbacks.push_back(cb); }
void WorldMapWidget::RemoveContextMenuCallback(ContextMenuCallback cb) { std::erase(context_menu_callbacks, cb); }
GW::Vec2f WorldMapWidget::GetContextMenuWorldMapPos() { return world_map_click_pos; }

void WorldMapWidget::AddOverlayCallback(OverlayCallback cb) { overlay_callbacks.push_back(cb); }
void WorldMapWidget::RemoveOverlayCallback(OverlayCallback cb) { std::erase(overlay_callbacks, cb); }

bool WorldMapWidget::WorldMapToScreen(const GW::Vec2f& world_map_pos, ImVec2& out)
{
    if (!(world_map_context && GW::UI::GetIsWorldMapShowing())) return false;
    const auto map_info = GW::Map::GetMapInfo(GW::Map::GetMapID());
    if (!(map_info && map_info->continent == world_map_context->continent)) return false;
    out = CalculateViewportPos(world_map_pos, world_map_context->top_left);
    return true;
}

float WorldMapWidget::GetPxPerWorldMapUnit() { return world_map_proj_scale.x; }

void WorldMapWidget::ShowAllOutposts(const bool show = settings.showing_all_outposts)
{
    if (view_all_outposts_patch.IsValid()) view_all_outposts_patch.TogglePatch(show);
    if (view_all_carto_areas_patch.IsValid()) view_all_carto_areas_patch.TogglePatch(show);
    TriggerWorldMapRedraw();
}

void WorldMapWidget::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWidget::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    for (size_t i = 0; i < _countof(show_elite_capture_locations); i++) {
        show_elite_capture_locations[i] = ((settings.show_elite_capture_locations_val >> i) & 0x1) != 0;
    }
    for (size_t i = 0; i < _countof(show_elite_capture_locations_campaign); i++) {
        show_elite_capture_locations_campaign[i] = ((settings.show_elite_capture_locations_campaign_val >> i) & 0x1) != 0;
    }
    ShowAllOutposts(settings.showing_all_outposts);


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
        // 连接传送门跨越地图，因此一旦整个文件加载完成，在第二遍中解析它们。
        for (auto& [_, info] : map_info_by_file_id) {
            for (auto& portal : info.portals) {
                portal.checkForLinkedPortal(info.continent);
            }
        }
    }
}

void WorldMapWidget::SaveSettings(SettingsDoc& doc)
{
    settings.show_elite_capture_locations_val = 0;
    for (size_t i = 0; i < _countof(show_elite_capture_locations); i++) {
        if (show_elite_capture_locations[i]) {
            settings.show_elite_capture_locations_val |= (1u << i);
        }
    }
    settings.show_elite_capture_locations_campaign_val = 0;
    for (size_t i = 0; i < _countof(show_elite_capture_locations_campaign); i++) {
        if (show_elite_capture_locations_campaign[i]) {
            settings.show_elite_capture_locations_campaign_val |= (1u << i);
        }
    }
    ToolboxWidget::SaveSettings(doc);
    doc.SetStruct(Name(), settings);

    const std::filesystem::path map_info_by_file_id_file = Resources::GetPath(L"MapInfoByFileId.txt");
    std::ofstream out(map_info_by_file_id_file);
    if (!out.is_open()) return;
    for (const auto& [file_id, info] : map_info_by_file_id) {
        out << "MAP " << static_cast<uint32_t>(info.map_id) << info.map_file_id << " " << info.world_pos_start.x << " " << info.world_pos_start.y << " " << info.world_pos_end.x << " " << info.world_pos_end.y << " " << info.portals.size() << " "
            << static_cast<uint32_t>(info.map_id) << "\n";
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
        bool carto_enabled = CartographerWidget::GetEnabled();
        if (ImGui::Checkbox("制图师", &carto_enabled)) {
            GW::GameThread::Enqueue([carto_enabled] {
                CartographerWidget::SetEnabled(carto_enabled);
            });
        }
        if (carto_enabled) {
            ImGui::Indent();
            CartographerWidget::DrawWorldMapOptions();
            ImGui::Unindent();
        }
        if (ImGui::Checkbox("显示所有区域", &settings.showing_all_outposts)) {
            GW::GameThread::Enqueue([] {
                ShowAllOutposts(settings.showing_all_outposts);
            });
        }
        if (settings.showing_all_outposts) {
            ImGui::Indent();
            ImGui::Checkbox("高亮锁定区域", &settings.highlight_locked_areas);
            if (settings.highlight_locked_areas) {
                ImGui::SameLine();
                ImGui::ColorButtonPicker("锁定区域", &settings.locked_area_highlight_color.value, ImGuiColorEditFlags_NoLabel);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("此角色未解锁区域的颜色叠加。");
                }
            }
            ImGui::Unindent();
        }
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
            bool is_hard_mode = GW::PartyMgr::GetIsPartyInHardMode();
            if (ImGui::Checkbox("困难模式", &is_hard_mode)) {
                GW::GameThread::Enqueue([] {
                    GW::PartyMgr::SetHardMode(!GW::PartyMgr::GetIsPartyInHardMode());
                });
            }
        }
        ImGui::Checkbox("在世界地图上显示工具箱小地图线", &settings.show_lines_on_world_map);
        if (ImGui::Checkbox("显示所有任务的任务标记", &settings.showing_all_quests)) {
            QuestModule::FetchMissingQuestInfo();
        }
        ImGui::Checkbox("应用任务标记颜色叠加", &settings.apply_quest_colors);
        if (settings.apply_quest_colors) {
            ImGui::Indent();
            auto color = &QuestModule::GetQuestColor((GW::Constants::QuestID)0xfff);
            ImGui::ColorButtonPicker("其他任务", color, ImGuiColorEditFlags_NoLabel);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("非激活任务的颜色叠加。");
            }
            if (GW::QuestMgr::GetActiveQuestId() != GW::Constants::QuestID::None) {
                ImGui::SameLine();
                color = &QuestModule::GetQuestColor(GW::QuestMgr::GetActiveQuestId());
                ImGui::ColorButtonPicker("激活任务", color, ImGuiColorEditFlags_NoLabel);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("激活任务的颜色叠加。");
                }
            }
            ImGui::Unindent();
        }
    }
    ImGui::Checkbox("显示精英技能获取位置", &settings.show_any_elite_capture_locations);
    if (settings.show_any_elite_capture_locations) {
        ImGui::Indent();
        constexpr const char* campaign_labels[] = {"核心", "预言", "派系", "夜幕"};
        constexpr const char* campaign_tooltips[] = {"核心", "预言", "派系", "夜幕"};
        for (size_t i = 0; i < _countof(show_elite_capture_locations_campaign); i++) {
            if (i != 0) ImGui::SameLine();
            ImGui::PushID(100 + (int)i);
            ImGui::PushStyleColor(ImGuiCol_Button, show_elite_capture_locations_campaign[i] ? completed_bg.Value : ImGui::GetStyleColorVec4(ImGuiCol_Button));
            if (ImGui::SmallButton(campaign_labels[i])) {
                show_elite_capture_locations_campaign[i] = !show_elite_capture_locations_campaign[i];
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", campaign_tooltips[i]);
            ImGui::PopID();
        }
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
        ImGui::Checkbox("隐藏已捕获的精英", &settings.hide_captured_elites);
        if (settings.hide_captured_elites) {
            const auto& completion = CompletionWindow::Instance().GetCharacterCompletion(GW::PlayerMgr::GetPlayerName(), false);
            if (!completion) ImGui::TextDisabled("如果完成窗口被禁用，则仅限于你的主/副职业");
        }
        ImGui::Checkbox("按职业为技能图标着色", &settings.color_elite_icons_by_profession);
        ImGui::Unindent();
    }
    ImGui::End();
    ImGui::PopStyleColor();


    if (window) {
        controls_window_rect = window->Rect();
        controls_window_rect.Translate(mouse_offset);
    }
    hovered_map_portal = 0;
#if 0
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
    DrawLockedAreaHighlights();
#endif



    hovered_boss = nullptr;
    locations_assigned_to_outposts.clear();
    for (auto& boss : elite_boss_locations) {
        if (DrawBossLocationOnWorldMap(boss)) {
            hovered_boss = &boss;
        }
    }

    hovered_quest_id = GW::Constants::QuestID::None;
    if (settings.showing_all_quests) {
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
            const auto coin_reward = DailyQuests::GetZaishenCoinReward(hovered_quest_id);
            if (coin_reward) {
                ImGui::SetTooltip("%s\n扎伊圣硬币：%u 普通 / %u 困难", quest_name.string().c_str(), coin_reward->nm, coin_reward->hm);
            }
            else {
                ImGui::SetTooltip("%s", quest_name.string().c_str());
            }
        }
    }
    if (hovered_boss) {
        ImGui::SetTooltip([&]() {
            if (settings.color_elite_icons_by_profession) {
                const auto skill = GW::SkillbarMgr::GetSkillConstantData(hovered_boss->skill_id);
                const auto prof_img = skill ? Resources::GetProfessionIcon(static_cast<GW::Constants::Profession>(skill->profession)) : 0;

                const auto sz = ImGui::CalcTextSize("").y * 1.5f;
                if (prof_img && *prof_img) {
                    ImGui::Image(*prof_img, {sz, sz});
                    ImGui::SameLine();
                }
            }
            ImGui::TextUnformatted(BossInfo(hovered_boss).c_str());
        });
    }
    if (hovered_map_portal) {
        ImGui::SetTooltip([]() {
            if (hovered_map_portal) DrawMapPortalInfo(hovered_map_portal);
        });
        
    }

    if (settings.show_lines_on_world_map) {
        const auto& lines = Minimap::Instance().custom_renderer.GetLines();
        const auto map_id = GW::Map::GetMapID();
        GW::Vec2f line_start;
        GW::Vec2f line_end;
        // 裁剪到可见视口：加载了许多传送门/路径线时，每帧将离屏线提交给 ImGui（顶点生成）是 FPS 瓶颈。
        // 廉价的屏幕空间 AABB 剔除只保留可见部分。
        const ImVec2 clip_min = draw_list->GetClipRectMin();
        const ImVec2 clip_max = draw_list->GetClipRectMax();
        for (auto& line : lines | std::views::filter([](auto line) {
                              return line->visible;
                          })) {
            if (line->map != map_id) continue;
            if (line->world_coords) {
                // 已经是世界地图坐标（例如跨地图路径尾部）— 直接使用。
                line_start = {line->p1.x, line->p1.y};
                line_end = {line->p2.x, line->p2.y};
            }
            else {
                if (!GamePosToWorldMap(line->p1, line_start)) continue;
                if (!GamePosToWorldMap(line->p2, line_end)) continue;
            }

            const auto p1 = CalculateViewportPos(line_start, world_map_context->top_left);
            const auto p2 = CalculateViewportPos(line_end, world_map_context->top_left);

            // 跳过屏幕空间边界框不与可见区域相交的段。
            if (std::max(p1.x, p2.x) < clip_min.x || std::min(p1.x, p2.x) > clip_max.x || std::max(p1.y, p2.y) < clip_min.y || std::min(p1.y, p2.y) > clip_max.y) continue;

            draw_list->AddLine(p1, p2, line->color);
        }

        if (GameWorldRenderer::GetNavmeshWorldMapMapId() == map_id) {
            for (const auto& e : GameWorldRenderer::GetNavmeshWorldMapLines()) {
                if (!GamePosToWorldMap(e.a, line_start)) continue;
                if (!GamePosToWorldMap(e.b, line_end)) continue;
                const auto p1 = CalculateViewportPos(line_start, world_map_context->top_left);
                const auto p2 = CalculateViewportPos(line_end, world_map_context->top_left);
                draw_list->AddLine(p1, p2, e.color);
            }
        }
    }
    if (settings.show_any_elite_capture_locations) {
        const auto rect = draw_list->GetClipRectMax();
        const auto text = "精英技能获取位置提取自 Aylee Sedai 的 MappingOut v4.0.0";
        draw_list->AddText({16.f, rect.y - 28.f}, ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
    }
    // 跨地图路径可能需要几秒在工作线程上构建；让玩家知道正在计算，而不是什么都没发生。
    // 位于 MappingOut 署名行上方（左下角）。
    if (PathfindingWindow::IsCalculatingPath()) {
        const auto rect = draw_list->GetClipRectMax();
        draw_list->AddText({16.f, rect.y - 48.f}, ImGui::GetColorU32(ImGuiCol_Text), "正在计算路径...");
    }
    for (const auto cb : overlay_callbacks) {
        cb(draw_list);
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
            // Resolve the click position before dispatching: the hovered-quest menu carries
            // contributed items that act on where the user clicked, so it needs it too.
            const bool have_click_pos = world_map_context->zoom == 1.0f;
            world_map_click_pos_valid = have_click_pos;
            if (have_click_pos) {
                world_map_click_pos = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                world_map_click_pos.x /= ui_scale.x;
                world_map_click_pos.y /= ui_scale.y;
                world_map_click_pos.x += world_map_context->top_left.x;
                world_map_click_pos.y += world_map_context->top_left.y;
            }
            if (GW::QuestMgr::GetQuest(hovered_quest_id)) {
                ImGui::SetContextMenu(HoveredQuestContextMenu, (void*)hovered_quest_id);
                break;
            }

            if (!have_click_pos) break;
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
    ImGui::TextDisabled("世界地图选项（显示所有区域、任务标记、精英技能位置等）\n请打开世界地图进行更改。");
}
