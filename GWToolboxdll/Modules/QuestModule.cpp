#include "stdafx.h"

#include "QuestModule.h"

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Quest.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <Modules/Resources.h>
#include <Widgets/Minimap/CustomRenderer.h>
#include <Widgets/Minimap/Minimap.h>
#include <Windows/Pathfinding/PathfindingWindow.h>
#include <Windows/TravelWindow.h>

#include <GWCA/Context/WorldContext.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Utilities/MemoryPatcher.h>

#include <Defines.h>
#include <Utils/GuiUtils.h>
#include <Utils/ToolboxUtils.h>
#include <Widgets/WorldMapWidget.h>
#include "AudioSettings.h"

namespace {
    QuestModule::Settings settings;

    // 任务路径依赖于（现为可选的）寻路模块；若无此模块，则无路由API可调用。
    // 无锁标志（非 GWToolbox::IsModuleEnabled）以保持在每帧 Update 路径上的轻量级。
    bool QuestPathingAvailable() { return PathfindingWindow::IsPathingEnabled(); }

    bool fetch_missing_quest_info_queued = false;
    // 任务消息和地图加载仅标记刷新；QuestModule::Update() 在加载屏幕和寻路保护之后消耗它，
    // 因此我们绝不会在转换期间对地图数据中启动路线计算。
    bool refresh_all_quest_paths_queued = false;

    std::vector<QuestModule::CustomMarkerChangedCallback> custom_marker_callbacks;

    GW::HookEntry pre_ui_message_entry;
    GW::HookEntry post_ui_message_entry;
    bool initialised = false;

    clock_t last_fetched_missing_quest_info = 0;

    GW::Constants::QuestID custom_quest_id = static_cast<GW::Constants::QuestID>(0x0000fdd);
    GW::Quest custom_quest_marker;
    GW::Vec2f custom_quest_marker_world_pos;
    GW::Constants::QuestID player_chosen_quest_id = GW::Constants::QuestID::None;
    bool setting_custom_quest_marker = false;

    clock_t last_quest_clicked = 0;

    GW::UI::UIInteractionCallback QuestLogRow_UICallback_Func = nullptr, QuestLogRow_UICallback_Ret = nullptr;

    // 如果双击任务条目，传送到最近的前哨站
    void OnQuestLogRow_UICallback(GW::UI::InteractionMessage* message, void* wParam, void* lParam)
    {
        GW::Hook::EnterHook();
        QuestLogRow_UICallback_Ret(message, wParam, lParam);
        if (!(settings.double_click_to_travel_to_quest && message->message_id == GW::UI::UIMessage::kMouseClick2 && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost &&
              GW::UI::BelongsToFrame(GW::UI::GetFrameByLabel(L"Quest"), GW::UI::GetFrameById(message->frame_id))))
            return GW::Hook::LeaveHook();
        const auto packet = (GW::UI::UIPacket::kMouseAction*)wParam;
        if (!(packet->current_state == GW::UI::UIPacket::ActionState::MouseClick && (packet->child_offset_id & 0xffff0000) == 0x80000000)) return GW::Hook::LeaveHook(); // 不是双击任务条目
        if (last_quest_clicked && TIMER_DIFF(last_quest_clicked) < 250) {
            const auto quest_id = static_cast<GW::Constants::QuestID>(packet->child_offset_id & 0xffff);
            const auto quest = GW::QuestMgr::GetQuest(quest_id);
            if (quest && quest->map_to != GW::Constants::MapID::Count) {
                TravelWindow::Instance().TravelNearest(quest->map_to);
            }
        }
        last_quest_clicked = TIMER_INIT();
        GW::Hook::LeaveHook();
    }

    void ClearCalculatedPath(GW::Constants::QuestID quest_id);
    struct CalculatedQuestPath;
    CalculatedQuestPath* GetCalculatedQuestPath(GW::Constants::QuestID quest_id, bool create_if_not_found);

    bool IsActiveQuestPath(const GW::Constants::QuestID quest_id)
    {
        const auto questlog = GW::QuestMgr::GetQuestLog();
        const auto active_quest = GW::QuestMgr::GetActiveQuest();
        if (!questlog || !active_quest) return false;
        if (quest_id == active_quest->quest_id) return true;
        if (!settings.show_paths_to_all_quests) return false;
        // 去重指向同一地点的其他任务！
        const auto quest = GW::QuestMgr::GetQuest(quest_id);
        if (!quest) return false; // 任务刚刚被移除？
        for (const auto q : *questlog) {
            if (quest->marker == q.marker) {
                return q.quest_id == quest_id;
            }
        }
        return false;
    }

    struct CalculatedQuestPath {
        CalculatedQuestPath(const GW::Constants::QuestID _quest_id) : quest_id(_quest_id) {}

        ~CalculatedQuestPath() { ClearMinimapLines(); }

        CalculatedQuestPath(const CalculatedQuestPath&) = delete;
        CalculatedQuestPath& operator=(const CalculatedQuestPath&) = delete;
        CalculatedQuestPath(CalculatedQuestPath&&) = delete;
        CalculatedQuestPath& operator=(CalculatedQuestPath&&) = delete;

        std::vector<CustomRenderer::CustomLine*> minimap_lines{};
        GW::Vec2f calculated_from{}; // 上次重算的游戏坐标锚点（用于移动检测）
        GW::Vec2f calculated_to{};   // 世界地图目标点
        clock_t calculated_at = 0;
        clock_t route_failed_at = 0; // 避免重试无法路由的标记
        clock_t last_check = 0;      // 每个路径的 Update() 节流（共享静态会饿死其他路径）
        GW::Constants::QuestID quest_id{};
        clock_t calculating = 0;
        GW::Vec2f goal_world{};
        bool has_full_route = false;
        bool goal_cross_map = false;
        std::vector<GW::Vec2f> route_world{}; // 世界地图坐标（地图之间的 PATH_BREAK）
        std::vector<GW::GamePos> route_map{};
        bool IsCalculating() { return calculating && TIMER_DIFF(calculating) < 5000; }

        void ClearMinimapLines()
        {
            for (const auto l : minimap_lines) {
                Minimap::Instance().custom_renderer.RemoveCustomLine(l);
            }
            minimap_lines.clear();
        }

        // 当前地图段绘制在地面表面；地图外段保持世界坐标（世界和任务地图）以避免溢出。
        void DrawLines()
        {
            ClearMinimapLines();
            if (!(settings.draw_quest_path_on_terrain || settings.draw_quest_path_on_minimap || settings.draw_quest_path_on_mission_map)) return;
            const auto color = QuestModule::GetQuestLineColor(quest_id);

            CustomRenderer::CustomLine* l = nullptr;
            bool first_ingame = true;
            for (size_t i = 0; i + 1 < route_map.size(); i++) {
                const auto label = std::format("{} - {}", static_cast<uint32_t>(quest_id), i);
                l = Minimap::Instance().custom_renderer.AddCustomLine(route_map[i], route_map[i + 1], label.c_str(), true);
                l->from_player_pos = first_ingame;
                l->draw_on_terrain = settings.draw_quest_path_on_terrain;
                l->draw_on_minimap = settings.draw_quest_path_on_minimap;
                l->draw_on_mission_map = settings.draw_quest_path_on_mission_map;
                first_ingame = false;
                l->created_by_toolbox = true;
                l->color = color;
                minimap_lines.push_back(l);
            }
            for (size_t i = 0; i + 1 < route_world.size(); i++) {
                if (PathfindingWindow::IsRouteBreak(route_world[i]) || PathfindingWindow::IsRouteBreak(route_world[i + 1])) continue;
                const auto label = std::format("{} - {}", static_cast<uint32_t>(quest_id), i);
                l = Minimap::Instance().custom_renderer.AddCustomLine({route_world[i].x, route_world[i].y, 0}, {route_world[i + 1].x, route_world[i + 1].y, 0}, label.c_str(), true);
                l->world_coords = true;
                l->draw_on_terrain = false;
                // 世界坐标在世界地图、任务地图和罗盘上渲染（每个都将其投影到各自空间）；世界地形无法放置其他地图的位置。
                l->draw_on_minimap = settings.draw_quest_path_on_minimap;
                l->draw_on_mission_map = settings.draw_quest_path_on_mission_map;
                l->created_by_toolbox = true;
                l->color = color;
                minimap_lines.push_back(l);
            }

            GameWorldRenderer::TriggerSyncAllMarkers();
        }

        [[nodiscard]] const GW::Quest* GetQuest() const { return GW::QuestMgr::GetQuest(quest_id); }

        [[nodiscard]] bool IsActive() const
        {
            const auto a = GW::QuestMgr::GetActiveQuestId() == quest_id;
            return a || (GetQuest() && Minimap::ShouldDrawAllQuests());
        }

        void RecalculateWorld(const GW::Vec2f& from_world)
        {
            const auto gw = goal_world;
            calculated_to = goal_world;
            Resources::EnqueueWorkerTask([qid = quest_id, from_world, gw = calculated_to] {
                auto pts = new std::vector<GW::Vec2f>(); // 世界地图坐标
                const bool ok = PathfindingWindow::CalculateRoute(from_world, gw, pts);
                auto route_map = new std::vector<GW::GamePos>(); // 游戏坐标
                if (ok) {
                    size_t route_map_end_idx;
                    GW::GamePos gp;
                    bool passed_route_break = false;
                    const auto data = pts->data();
                    for (route_map_end_idx = 0; route_map_end_idx < pts->size(); route_map_end_idx++) {
                        if (PathfindingWindow::IsRouteBreak(data[route_map_end_idx])) {
                            // 路线中断（例如传送门）：仍绘制下一点以便玩家通过。
                            passed_route_break = true;
                            continue;
                        }
                        // @cleanup: 此操作在工作线程上安全吗？！
                        if (!(PathfindingWindow::IsWorldPosOnMap(data[route_map_end_idx]) && WorldMapWidget::WorldMapToGamePos(data[route_map_end_idx], gp))) break;
                        route_map->push_back(gp);
                        if (passed_route_break) break;
                    }
                    if (route_map_end_idx) pts->erase(pts->begin(), pts->begin() + route_map_end_idx);
                }
                Resources::EnqueueMainTask([qid, route_map, pts, ok, gw] {
                    const auto cqp = GetCalculatedQuestPath(qid, false);
                    if (cqp && cqp->goal_world != gw) {
                        // 计算过程中标记移动了 — 此路线属于旧目标。丢弃它并强制为当前目标重新规划（设置 calculated_at=0 使 Update 在下一帧无论玩家移动与否都重新计算）。
                        cqp->calculating = 0;
                        cqp->calculated_at = 0;
                    }
                    else if (cqp && ok) {
                        cqp->route_world = std::move(*pts);
                        cqp->route_map = std::move(*route_map);
                        cqp->has_full_route = true;
                        if (const auto self = GW::Agents::GetControlledCharacter()) cqp->TrimLeadingWaypoints(self->pos);
                        cqp->calculated_at = TIMER_INIT();
                        cqp->route_failed_at = 0;
                        cqp->calculating = 0;
                        cqp->UpdateUI();
                    }
                    else if (cqp) {
                        cqp->calculating = 0;
                        cqp->calculated_at = TIMER_INIT();
                        cqp->route_failed_at = TIMER_INIT();
                    }
                    delete pts;
                    delete route_map;
                });
            });
        }

        void RecalculateMap(const GW::GamePos& from)
        {
            // 直接单地图 A* 到地图内端点（目标点，或路线离开此地图的位置）。
            const bool same_map = PathfindingWindow::IsWorldPosOnMap(goal_world) && !goal_cross_map;
            GW::GamePos target{};
            if (same_map) {
                if (!WorldMapWidget::WorldMapToGamePos(goal_world, target)) {
                    // Nothing enqueued, so clear the in-flight flag — leaving it set blocks every Update for 5s.
                    calculating = 0;
                    calculated_at = 0;
                    return;
                }
            }
            else {
                if (route_world.empty() || route_map.empty()) {
                    // No cross-map route to walk toward: bailing left `calculating` set with no work queued, freezing the path.
                    has_full_route = false;
                    GW::Vec2f from_world{};
                    WorldMapWidget::GamePosToWorldMap(from, from_world);
                    RecalculateWorld(from_world);
                    return;
                }
                target = route_map.back();
            }
            // RecalculateSegment's game-coord leg keeps the A* per-waypoint zplane (a world-coord round-trip would zero it), so the line drapes on the real surface.
            Resources::EnqueueWorkerTask([qid = quest_id, from, target, same_map, gw = goal_world] {
                auto pts = new std::vector<GW::Vec2f>();         // required out-param; unused here
                auto route_map = new std::vector<GW::GamePos>(); // current-map game coords with carried zplane
                const bool ok = PathfindingWindow::RecalculateSegment(static_cast<GW::Constants::MapID>(0), from, target, pts, route_map);
                Resources::EnqueueMainTask([qid, from, route_map, pts, ok, same_map, gw] {
                    const auto cqp = GetCalculatedQuestPath(qid, false);
                    if (cqp && cqp->goal_world != gw) {
                        // 计算过程中标记移动了 — 旧段已过时。丢弃它并强制为当前目标重新规划。
                        cqp->calculating = 0;
                        cqp->calculated_at = 0;
                        delete pts;
                        delete route_map;
                        return;
                    }
                    if (cqp && ok) {
                        cqp->route_map = std::move(*route_map);
                        if (same_map) {
                            cqp->route_world.clear(); // 整条路线都在地图内
                            cqp->has_full_route = true;
                        }
                        if (const auto self = GW::Agents::GetControlledCharacter()) cqp->TrimLeadingWaypoints(self->pos);
                        cqp->calculated_at = TIMER_INIT();
                        cqp->route_failed_at = 0;
                        cqp->calculating = 0;
                        cqp->UpdateUI();
                    }
                    else if (cqp && same_map) {
                        cqp->goal_cross_map = true;
                        GW::Vec2f from_world{};
                        WorldMapWidget::GamePosToWorldMap(from, from_world);
                        cqp->calculating = TIMER_INIT();
                        cqp->RecalculateWorld(from_world);
                    }
                    else if (cqp) {
                        cqp->calculating = 0;
                        cqp->calculated_at = TIMER_INIT();
                        cqp->route_failed_at = TIMER_INIT();
                    }
                    delete pts;
                    delete route_map;
                });
            });
        }

        // 一次计算到 goal_world 的完整路线；后续移动仅重走当前地图段，其余部分保持不变。
        void Recalculate(const GW::GamePos& from)
        {
            if (IsCalculating()) return;
            if (!QuestPathingAvailable() || !PathfindingWindow::ReadyForPathing()) {
                calculating = 0;
                calculated_at = 0;
                return;
            }
            GW::Vec2f from_world{};
            WorldMapWidget::GamePosToWorldMap(from, from_world);
            const GW::Vec2f from_game{from.x, from.y};

            if (calculated_from == from_game && calculated_to == goal_world) return;

            calculating = TIMER_INIT();
            const bool goal_changed = calculated_to != goal_world;
            if (goal_changed) {
                goal_cross_map = false; // new goal — re-test whether it's reachable on the current map
                has_full_route = false; // and plot it from scratch instead of re-walking the old route's leg
            }
            calculated_from = from_game; // anchor for Update's move threshold
            calculated_to = goal_world;
            const bool same_map = PathfindingWindow::IsWorldPosOnMap(goal_world) && !goal_cross_map;
            // Gate on the route we hold, not on `goal_changed`: a failed plot otherwise left us re-walking a leg that no longer exists.
            if (!same_map && !has_full_route) {
                RecalculateWorld(from_world);
            }
            else {
                RecalculateMap(from);
            }
        }

        // Drop points we've walked past; the drawn head starts at the live player pos, so anything behind us draws backwards.
        bool TrimLeadingWaypoints(const GW::GamePos& from)
        {
            bool dropped = false;
            while (route_map.size() > 2) {
                const GW::GamePos a = route_map[0], b = route_map[1];
                const float segx = b.x - a.x, segy = b.y - a.y;
                const float len2 = segx * segx + segy * segy;
                const float t = len2 > 0.f ? ((from.x - a.x) * segx + (from.y - a.y) * segy) / len2 : 1.f;
                // Also drop when standing on b: walking off the line keeps the projection short of 1 and pins the head to a reached waypoint.
                constexpr float waypoint_reached_sqr = 166.f * 166.f; // adjacent range
                const float bx = from.x - b.x, by = from.y - b.y;
                const bool reached = bx * bx + by * by < waypoint_reached_sqr;
                if (t < 1.f && !reached) break; // route_map[1] still ahead
                route_map.erase(route_map.begin());
                dropped = true;
            }
            return dropped;
        }

        bool Update(const GW::GamePos& from)
        {
            // 每路径节流（成员变量，非共享静态 — 共享静态会饿死所有其他路径的重算）。
            if (TIMER_DIFF(last_check) < 33) return false;
            last_check = TIMER_INIT();

            if (IsCalculating()) return false;
            if (!GetQuest()) {
                ClearCalculatedPath(quest_id);
                return true;
            }

            if (!calculated_at) {
                Recalculate(from);
                return false;
            }
            if (TrimLeadingWaypoints(from)) UpdateUI();
            // 无法路由的标记：退避而不是每帧重新计算（并重新记录）；目标/地图更改通过 RefreshQuestPath 重新规划。
            constexpr clock_t failed_route_backoff = 10000;
            if (route_failed_at && TIMER_DIFF(route_failed_at) < failed_route_backoff) return false;
            // 一旦玩家移动了配置的“路径重算距离”就从锚点重算。
            const GW::Vec2f from_game{from.x, from.y};
            const float d = PathfindingWindow::GetPathRecalcDistance();
            if (GetSquareDistance(from_game, calculated_from) > d * d) Recalculate(from);
            return false;
        }

        void UpdateUI() { DrawLines(); }
    };

    void BlockQuestSound()
    {
        AudioSettings::BlockSoundForMs(L"\xe14d\x0101", 1000);
        AudioSettings::BlockSoundForMs(L"\xe14c\x0101", 1000);
    }


    // 自定义任务ID大于游戏内计数 — 有些断言不喜欢这样！
    GW::MemoryPatcher bypass_custom_quest_assertion_patch;

    std::unordered_map<GW::Constants::QuestID, CalculatedQuestPath*> calculated_quest_paths;

    void ClearCalculatedPaths()
    {
        for (const auto cqp : calculated_quest_paths | std::views::values) {
            delete cqp;
        }
        calculated_quest_paths.clear();
    }

    void ClearCalculatedPath(GW::Constants::QuestID quest_id)
    {
        const auto found = calculated_quest_paths.find(quest_id);
        if (found == calculated_quest_paths.end()) return;
        auto cqp = found->second;
        calculated_quest_paths.erase(found);
        delete cqp;
    }

    CalculatedQuestPath* GetCalculatedQuestPath(GW::Constants::QuestID quest_id, bool create_if_not_found = true)
    {
        const auto found = calculated_quest_paths.find(quest_id);
        if (found != calculated_quest_paths.end()) return found->second;
        if (!create_if_not_found) return nullptr;
        const auto cqp = new CalculatedQuestPath(quest_id);
        calculated_quest_paths[quest_id] = cqp;
        return cqp;
    }

    bool is_spoofing_quest_update = false;

    GW::GamePos* GetPlayerPos()
    {
        const auto p = GW::Agents::GetControlledCharacter();
        return p ? &p->pos : nullptr;
    }

    float GetSquareDistance(const GW::GamePos& a, const GW::GamePos& b)
    {
        return GetSquareDistance(static_cast<GW::Vec2f>(a), static_cast<GW::Vec2f>(b));
    }

    void RefreshQuestPath(GW::Constants::QuestID quest_id)
    {
        GW::GameThread::Enqueue([quest_id] {
            if (!QuestPathingAvailable() || !IsActiveQuestPath(quest_id)) {
                ClearCalculatedPath(quest_id);
                return;
            }
            const auto quest = GW::QuestMgr::GetQuest(quest_id);
            const auto pos = quest ? GetPlayerPos() : nullptr;
            if (!pos) return;
            const auto cqp = GetCalculatedQuestPath(quest_id);
            if (!cqp) return;

            // 解析世界地图目标：自定义标记拥有其世界位置；常规任务使用地图内标记，但当跨地图且地图内标记无效或距离过远（>5000 格林奇）时，回退到目标地图的世界标记。
            GW::Vec2f goal{};
            bool have_goal = false;
            if (quest_id == custom_quest_id) {
                goal = custom_quest_marker_world_pos;
                have_goal = goal.x != 0 || goal.y != 0;
            }
            else {
                const bool marker_valid = quest->marker.x != INFINITY;
                const bool cross_map = quest->map_to != GW::Map::GetMapID() && quest->map_to != GW::Constants::MapID::None;
                const bool marker_far = marker_valid && GetSquareDistance(*pos, quest->marker) > 5000.f * 5000.f;
                if (cross_map && (!marker_valid || marker_far))
                    have_goal = WorldMapWidget::GetMapMarkerWorldPos(quest->map_to, goal);
                else if (marker_valid)
                    have_goal = WorldMapWidget::GamePosToWorldMap(quest->marker, goal);
            }
            if (!have_goal) {
                cqp->route_world.clear();
                cqp->route_map.clear();
                cqp->has_full_route = false;
                cqp->calculated_at = 0;
                cqp->route_failed_at = 0;
                cqp->UpdateUI();
                return;
            }
            if (GetSquareDistance(goal, cqp->goal_world) > 10.f * 10.f) {
                cqp->has_full_route = false; // 目标移动 → 重新规划整条路线
                cqp->route_failed_at = 0;    // 并为新目标提供干净的重试
            }
            cqp->goal_world = goal;
            cqp->Recalculate(*pos);
        });
    }

    void ClearCalculatedQuestPaths()
    {
        for (auto quest_path : calculated_quest_paths | std::views::values) {
            delete quest_path;
        }
        calculated_quest_paths.clear();
    }

    GW::Constants::QuestID quest_id_before_map_load = GW::Constants::QuestID::None;

    void RefreshAllQuestPaths()
    {
        const auto q = GW::QuestMgr::GetQuestLog();
        if (!q) return;
        for (auto& quest : *q) {
            RefreshQuestPath(quest.quest_id);
        }
    }

    void OnPreUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*)
    {
        switch (message_id) {
            case GW::UI::UIMessage::kQuestAdded: {
                const auto quest = GW::QuestMgr::GetQuest(*(GW::Constants::QuestID*)wparam);
                if (!quest) break;
                if (quest->quest_id == custom_quest_id) {
                    quest->log_state |= 1; // 避免询问此任务的描述
                }
            } break;
            case GW::UI::UIMessage::kStartMapLoad: {
                const auto q = GW::QuestMgr::GetActiveQuestId();
                if (q != GW::Constants::QuestID::None) quest_id_before_map_load = q;
            } break;
            case GW::UI::UIMessage::kSendSetActiveQuest: {
                const auto quest_id = static_cast<GW::Constants::QuestID>((uint32_t)wparam);
                if (setting_custom_quest_marker) {
                    // 如果玩家没有任务，或地图刚加载，触发此操作；我们希望通过伪造先前的任务选择（如果有的话）来“撤销”此操作。
                    status->blocked = true;
                    QuestModule::SetActiveQuestId(GW::QuestMgr::GetActiveQuestId(), false);
                    return;
                }
                player_chosen_quest_id = quest_id;
                if (quest_id == custom_quest_id) {
                    // 如果玩家选择了自定义任务，伪造响应而不询问服务器
                    status->blocked = true;
                    QuestModule::SetActiveQuestId(quest_id, false);
                }
            } break;
            case GW::UI::UIMessage::kSendAbandonQuest: {
                const auto quest_id = static_cast<GW::Constants::QuestID>((uint32_t)wparam);
                if (quest_id == custom_quest_id) {
                    status->blocked = true;
                    QuestModule::SetCustomQuestMarker({0, 0});
                }
            } break;
            case GW::UI::UIMessage::kOnScreenMessage: {
                // 放置自定义标记时阻止屏幕消息
                if (setting_custom_quest_marker) {
                    status->blocked = true;
                }
            } break;
        }
    }

    // 由任务相关的UI消息调用的回调。发送的所有消息都应将任务ID作为第一个wparam变量。
    void OnPostUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* packet, void*)
    {
        if (status->blocked) return;
        switch (message_id) {
            case GW::UI::UIMessage::kQuestDetailsChanged:
            case GW::UI::UIMessage::kQuestAdded:
            case GW::UI::UIMessage::kClientActiveQuestChanged: {
                const auto quest = GW::QuestMgr::GetQuest(*(GW::Constants::QuestID*)packet);
                if (quest && settings.keep_current_quest_when_new_quest_added && quest->quest_id != player_chosen_quest_id && GW::QuestMgr::GetQuest(player_chosen_quest_id)) {
                    // 未经用户交互分配的任务
                    QuestModule::SetActiveQuestId(player_chosen_quest_id, true);
                }
                refresh_all_quest_paths_queued = true;
            } break;
            case GW::UI::UIMessage::kServerActiveQuestChanged:
                refresh_all_quest_paths_queued = true;
                break;
            case GW::UI::UIMessage::kMapLoaded:
                BlockQuestSound();
                break;
        }
    }

    bool was_loading = true;


    void OnMapLoaded()
    {
        if (GW::UI::IsLoadingScreenShown()) return;
        BlockQuestSound();
        QuestModule::FetchMissingQuestInfo();
        ClearCalculatedQuestPaths();
        if (custom_quest_marker_world_pos.y != 0 || custom_quest_marker_world_pos.x != 0) {
            QuestModule::SetCustomQuestMarker(custom_quest_marker_world_pos, quest_id_before_map_load == custom_quest_id);
        }
        refresh_all_quest_paths_queued = true;
    }

    // 危险地释放GW内存！
    void RemoveQuest(GW::Constants::QuestID quest_id)
    {
        auto* quest = GW::QuestMgr::GetQuest(quest_id);
        if (!quest) return;
        auto w = GW::GetWorldContext();
        auto& quest_log = w->quest_log;
        // Grab the owned buffers before the shift below overwrites this slot; otherwise
        // we'd free the trailing duplicate's pointers, which the last live quest still uses.
        wchar_t* const owned[] = {quest->objectives, quest->description, quest->npc, quest->name, quest->location};

        const auto idx = quest - quest_log.m_buffer;
        const auto remaining = quest_log.m_size - idx - 1;
        if (remaining > 0) memmove(quest, quest + 1, remaining * sizeof(*quest_log.m_buffer));
        if (w->active_quest_id == quest_id) {
            w->active_quest_id = GW::Constants::QuestID::None;
        }
        quest_log.m_size--;
        // Clear the vacated slot so its stale copy can't alias the live quest's buffers.
        memset(&quest_log.m_buffer[quest_log.m_size], 0, sizeof(*quest_log.m_buffer));

        for (auto* buf : owned) {
            GW::MemoryMgr::MemFree(buf);
        }

        GW::UI::SendUIMessage(GW::UI::UIMessage::kMessage_0x10000152, (void*)&quest_id);
    }

    // 危险地分配GW内存！
    GW::Quest* AddQuest(
        GW::Constants::QuestID quest_id, GW::Constants::MapID map_from, GW::Constants::MapID map_to, uint32_t log_state, const GW::GamePos& marker, const wchar_t* name = 0, const wchar_t* location = 0, const wchar_t* description = 0,
        const wchar_t* npc = 0, const wchar_t* objectives = 0
    )
    {
        RemoveQuest(quest_id);
        auto& quest_log = GW::GetWorldContext()->quest_log;
        if (quest_log.m_capacity == quest_log.m_size) {
            auto* new_buf = (GW::Quest*)GW::MemoryMgr::MemRealloc(quest_log.m_buffer, (quest_log.m_size + 1) * sizeof(*quest_log.m_buffer));
            ASSERT(new_buf);
            quest_log.m_buffer = new_buf;
            quest_log.m_capacity++;
        }
        auto* quest = &quest_log.m_buffer[quest_log.m_size];
        memset(quest, 0, sizeof(*quest));
        quest->quest_id = quest_id;
        quest->log_state = log_state | 0x1;
        quest->map_from = map_from;
        quest->map_to = map_to;
        quest->marker = marker;
        quest_log.m_size++;

        auto write_wchar_buf = [](wchar_t** dest, const wchar_t* src) {
            if (!(src && *src)) return;
            const auto bytes = (wcslen(src) + 1) * sizeof(*src);
            *dest = (wchar_t*)GW::MemoryMgr::MemAlloc(bytes);
            ASSERT(*dest);
            swprintf(*dest, bytes / sizeof(*src), src);
        };
        write_wchar_buf(&quest->name, name);
        write_wchar_buf(&quest->location, location);
        write_wchar_buf(&quest->description, description);
        write_wchar_buf(&quest->npc, npc);
        write_wchar_buf(&quest->objectives, objectives);

        GW::UI::UIPacket::kServerActiveQuestChanged packet = {.quest_id = quest->quest_id, .marker = quest->marker, .h0024 = quest->h0024, .map_id = quest->map_to, .log_state = quest->log_state};
        GW::UI::SendUIMessage(GW::UI::UIMessage::kQuestAdded, &packet);
        GW::UI::SendUIMessage(GW::UI::UIMessage::kQuestDetailsChanged, &packet);

        return quest;
    }
} // namespace

const GW::Quest* QuestModule::GetCustomQuestMarker()
{
    return GW::QuestMgr::GetQuest(custom_quest_id);
}

bool QuestModule::GetCustomQuestMarkerWorldPos(GW::Constants::QuestID quest_id, GW::Vec2f& out)
{
    if (quest_id != custom_quest_id) return false;
    if (custom_quest_marker_world_pos.x == 0 && custom_quest_marker_world_pos.y == 0) return false;
    out = custom_quest_marker_world_pos;
    return true;
}

void QuestModule::SetCustomQuestMarker(const GW::Vec2f& world_pos, bool set_active)
{
    BlockQuestSound();
    Instance().Initialize();
    if (!GW::Agents::GetControlledCharacter()) return; // 地图未就绪

    custom_quest_marker_world_pos = world_pos;

    RemoveQuest(custom_quest_id);

    if (custom_quest_marker_world_pos.x == 0 && custom_quest_marker_world_pos.y == 0) {
        for (const auto& cb : custom_marker_callbacks)
            cb();
        return;
    }

    setting_custom_quest_marker = true;

    GW::GamePos marker = {INFINITY, INFINITY};
    const auto map_to = WorldMapWidget::GetMapIdForLocation(custom_quest_marker_world_pos);
    if (map_to == GW::Map::GetMapID()) WorldMapWidget::WorldMapToGamePos(custom_quest_marker_world_pos, marker);

    auto* quest = AddQuest(
        custom_quest_id, GW::Constants::MapID::Count, map_to, 0x20, marker,
        L"\x452", // "地图旅行"
        L"\x564", // "主要任务"
        L"\x108\x107您已在地图上放置了一个自定义标记。\x1"
    );
    ASSERT(quest);
    if (set_active) {
        QuestModule::SetActiveQuestId(quest->quest_id, false);
    }

    // 路线（可能跨地图）从玩家绘制到此世界位置，并由 CalculatedQuestPath 拥有；
    // 为其设置目标并强制全新整条路线规划。
    if (auto* cqp = GetCalculatedQuestPath(custom_quest_id)) {
        cqp->goal_world = custom_quest_marker_world_pos;
        cqp->has_full_route = false;
    }

    setting_custom_quest_marker = false;
    RefreshQuestPath(custom_quest_id);
    for (const auto& cb : custom_marker_callbacks)
        cb();
}

void QuestModule::ClearCustomQuestMarker()
{
    SetCustomQuestMarker({0, 0});
}

void QuestModule::AddCustomMarkerChangedCallback(CustomMarkerChangedCallback cb)
{
    custom_marker_callbacks.push_back(cb);
}

void QuestModule::RemoveCustomMarkerChangedCallback(CustomMarkerChangedCallback cb)
{
    std::erase(custom_marker_callbacks, cb);
}

std::vector<QuestObjective> QuestModule::ParseQuestObjectives(GW::Constants::QuestID quest_id)
{
    const auto quest = GW::QuestMgr::GetQuest(quest_id);
    std::vector<QuestObjective> out;
    if (!quest) return out;
    const wchar_t* next_objective_enc = nullptr;
    const wchar_t* current_objective_enc = quest->objectives;
    if (!quest->objectives) {
        if (quest_id == custom_quest_id) return out;
        BlockQuestSound();
        GW::QuestMgr::RequestQuestInfo(quest);
    }

    while (current_objective_enc) {
        next_objective_enc = wcschr(current_objective_enc, 0x2);
        size_t current_objective_len = next_objective_enc ? next_objective_enc - current_objective_enc : wcslen(current_objective_enc);

        auto enc_str = std::wstring(current_objective_enc, current_objective_len);
        auto content_start = enc_str.find(0x10a);
        if (content_start == std::wstring::npos) break;
        content_start++;

        enc_str = enc_str.substr(content_start, enc_str.size() - content_start - 1);

        const auto is_complete = *current_objective_enc == 0x2af5;

        out.push_back({quest_id, enc_str.c_str(), is_complete});

        current_objective_enc = next_objective_enc ? next_objective_enc + 1 : nullptr;
    }
    return out;
}

ImU32& QuestModule::GetQuestColor(GW::Constants::QuestID quest_id)
{
    if (GW::QuestMgr::GetActiveQuestId() == quest_id) {
        return Minimap::Instance().symbols_renderer.color_quest;
    }
    return Minimap::Instance().symbols_renderer.color_other_quests;
}

ImU32& QuestModule::GetQuestLineColor(GW::Constants::QuestID quest_id)
{
    if (GW::QuestMgr::GetActiveQuestId() == quest_id) {
        return Minimap::Instance().symbols_renderer.color_quest_line;
    }
    return Minimap::Instance().symbols_renderer.color_other_quests;
}

void QuestModule::DrawSettingsInternal()
{
    ImGui::CheckboxWithHelp(
        "接受新任务时保持当前任务", &settings.keep_current_quest_when_new_quest_added,
        "默认情况下，激战会将您当前选中的任务更改为您刚从NPC处接受的任务。\n如果您没有注意到任务标记现在将您带往新的目的地，这可能会很烦人！\n勾选此选项可确保在向任务日志添加新任务时也不会更改当前任务指引。"
    );
    ImGui::Checkbox("在任务日志窗口中双击任务以传送到最近的前哨站", &settings.double_click_to_travel_to_quest);
    if (QuestPathingAvailable()) {
        ImGui::Text("在以下位置绘制通往任务标记的路径：");
        bool recalc_quest_paths = false;
        recalc_quest_paths |= ImGui::Checkbox("地面##terrianquestpath", &settings.draw_quest_path_on_terrain);
        recalc_quest_paths |= ImGui::Checkbox("小地图##minimapquestpath", &settings.draw_quest_path_on_minimap);
        recalc_quest_paths |= ImGui::Checkbox("任务地图##missionmapquestpath", &settings.draw_quest_path_on_mission_map);
        ImGui::Checkbox("世界地图##worldmapquestpath", &WorldMapWidget::ShowLinesOnWorldMap());
#ifdef _DEBUG
        recalc_quest_paths |= ImGui::Checkbox("显示所有任务的路径##drawallquestpaths", &settings.show_paths_to_all_quests);
#endif
        if (recalc_quest_paths) RefreshAllQuestPaths();
    }
    else {
        ImGui::TextDisabled("启用寻路模块以绘制通往任务标记的路径。");
    }
}

void QuestModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    custom_quest_marker_world_pos = {settings.custom_quest_marker_world_pos_x, settings.custom_quest_marker_world_pos_y};
    GW::GameThread::Enqueue([] {
        SetCustomQuestMarker(custom_quest_marker_world_pos);
    });
}

void QuestModule::SaveSettings(SettingsDoc& doc)
{
    settings.custom_quest_marker_world_pos_x = custom_quest_marker_world_pos.x;
    settings.custom_quest_marker_world_pos_y = custom_quest_marker_world_pos.y;
    ToolboxModule::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

void QuestModule::Initialize()
{
    if (initialised) return;
    initialised = true;
    ToolboxModule::Initialize();
    SettingsRegistry::Register(this, settings);

    memset(&custom_quest_marker, 0, sizeof(custom_quest_marker));
    uintptr_t address = GW::Scanner::FindAssertion("UiCtlWebLink.cpp", "challengeId < CHALLENGES", 0, -0x7);
    DEBUG_ASSERT(address);
    if (address) {
        bypass_custom_quest_assertion_patch.SetPatch(address, "\xeb", 1);
        bypass_custom_quest_assertion_patch.TogglePatch(true);
    }

    constexpr GW::UI::UIMessage ui_messages[] = {GW::UI::UIMessage::kQuestDetailsChanged,      GW::UI::UIMessage::kQuestAdded,       GW::UI::UIMessage::kClientActiveQuestChanged,
                                                 GW::UI::UIMessage::kServerActiveQuestChanged, GW::UI::UIMessage::kMapLoaded,        GW::UI::UIMessage::kOnScreenMessage,
                                                 GW::UI::UIMessage::kSendSetActiveQuest,       GW::UI::UIMessage::kSendAbandonQuest, GW::UI::UIMessage::kStartMapLoad};
    for (const auto ui_message : ui_messages) {
        // 后回调，非阻塞
        RegisterUIMessageCallback(&pre_ui_message_entry, ui_message, OnPreUIMessage, -0x4000);
        RegisterUIMessageCallback(&post_ui_message_entry, ui_message, OnPostUIMessage, 0x4000);
    }
    RefreshQuestPath(GW::QuestMgr::GetActiveQuestId());

    QuestLogRow_UICallback_Func = (GW::UI::UIInteractionCallback)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("CtlFrameList.cpp", "selection", 0, 0), 0xfff);

    if (QuestLogRow_UICallback_Func) {
        GW::Hook::CreateHook((void**)&QuestLogRow_UICallback_Func, OnQuestLogRow_UICallback, (void**)&QuestLogRow_UICallback_Ret);
        GW::Hook::EnableHooks(QuestLogRow_UICallback_Func);
    }

#ifdef _DEBUG
    ASSERT(QuestLogRow_UICallback_Func);
    ASSERT(bypass_custom_quest_assertion_patch.GetAddress());
#endif
}

bool QuestModule::SetActiveQuestId(GW::Constants::QuestID quest_id, bool notify_server)
{
    Instance().Initialize();
    const auto quest = GW::QuestMgr::GetQuest(quest_id);
    if (!quest) {
        return false;
    }
    BlockQuestSound();
    if (quest_id == custom_quest_id) notify_server = false;

    if (notify_server) {
        GW::QuestMgr::SetActiveQuestId(quest_id);
    }
    else {
        GW::UI::UIPacket::kServerActiveQuestChanged packet = {.quest_id = quest->quest_id, .marker = quest->marker, .h0024 = quest->h0024, .map_id = quest->map_to, .log_state = quest->log_state};
        GW::GetWorldContext()->active_quest_id = quest->quest_id;
        GW::UI::SendUIMessage(GW::UI::UIMessage::kServerActiveQuestChanged, &packet);
    }
    return true;
}

void QuestModule::SignalTerminate()
{
    ToolboxModule::SignalTerminate();
    GW::GameThread::Enqueue([] {
        SetCustomQuestMarker({0, 0});
    });
    GW::UI::RemoveUIMessageCallback(&pre_ui_message_entry);
    GW::UI::RemoveUIMessageCallback(&post_ui_message_entry);
    ClearCalculatedPaths();
    if (QuestLogRow_UICallback_Func) {
        GW::Hook::RemoveHook(QuestLogRow_UICallback_Func);
    }
    if (bypass_custom_quest_assertion_patch.IsValid()) bypass_custom_quest_assertion_patch.Reset();
}

void QuestModule::Update(float)
{
    const auto pos = GetPlayerPos();
    if (!pos) {
        was_loading = true;
        return;
    }
    if (was_loading) {
        if (GW::UI::IsLoadingScreenShown()) return;
        OnMapLoaded();
        was_loading = false;
    }
    if (fetch_missing_quest_info_queued) {
        // 注意：仅在加载屏幕消失后执行此操作
        fetch_missing_quest_info_queued = 0;
        GW::GameThread::Enqueue([] {
            const auto quest_log = GW::QuestMgr::GetQuestLog();
            const auto active_quest = GW::QuestMgr::GetActiveQuestId();
            if (!quest_log) return;
            BlockQuestSound();
            for (auto& quest : *quest_log) {
                if ((quest.log_state & 1)) continue;
                GW::QuestMgr::RequestQuestInfoId(quest.quest_id, true);
            }
            SetActiveQuestId(active_quest);
        });
    }


    // 寻路可在运行时关闭；丢弃我们原本会不断重绘的路线。
    if (!QuestPathingAvailable()) {
        if (!calculated_quest_paths.empty()) ClearCalculatedPaths();
        return;
    }

    // 消耗由任务消息/地图加载标记的任何刷新。此处已越过加载屏幕和寻路保护，
    // 因此这里是启动路线计算的唯一位置 — 绝不会在转换期间对地图数据中启动。
    if (refresh_all_quest_paths_queued) {
        refresh_all_quest_paths_queued = false;
        RefreshAllQuestPaths();
    }

    const size_t size = calculated_quest_paths.size();
check_paths:
    for (const auto& [quest_id, calculated_quest_path] : calculated_quest_paths) {
        if (!IsActiveQuestPath(quest_id)) {
            ClearCalculatedPath(quest_id);
            ASSERT(size != calculated_quest_paths.size());
            goto check_paths;
        }
        if (calculated_quest_path->Update(*pos)) {
            ASSERT(size != calculated_quest_paths.size());
            goto check_paths;
        }
    }
}

bool QuestModule::CanTerminate()
{
    return !GetCustomQuestMarker();
}

void QuestModule::FetchMissingQuestInfo()
{
    if (fetch_missing_quest_info_queued) return;
    fetch_missing_quest_info_queued = TIMER_INIT();
}

void QuestModule::Terminate()
{
    ToolboxModule::Terminate();
    initialised = false;
}

QuestObjective::QuestObjective(const GW::Constants::QuestID quest_id, const wchar_t* objective_enc, const bool is_completed) : quest_id(quest_id), objective_enc(std::make_unique<GuiUtils::EncString>(objective_enc)), is_completed(is_completed) {}
