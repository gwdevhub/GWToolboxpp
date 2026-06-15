#include "stdafx.h"

#include "QuestModule.h"

#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <Windows/TravelWindow.h>
#include <Windows/Pathfinding/PathfindingWindow.h>
#include <Widgets/Minimap/CustomRenderer.h>
#include <Widgets/Minimap/Minimap.h>
#include <Modules/Resources.h>

#include <Utils/GuiUtils.h>
#include <GWCA/Context/WorldContext.h>
#include <Widgets/WorldMapWidget.h>
#include <GWCA/Utilities/MemoryPatcher.h>
#include <Utils/ToolboxUtils.h>
#include "AudioSettings.h"
#include <GWCA/Managers/MemoryMgr.h>

namespace {
    QuestModule::Settings settings;

    bool fetch_missing_quest_info_queued = false;

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

    // If double clicked on a quest entry, teleport to nearest outpost
    void OnQuestLogRow_UICallback(GW::UI::InteractionMessage* message, void* wParam, void* lParam)
    {
        GW::Hook::EnterHook();
        QuestLogRow_UICallback_Ret(message, wParam, lParam);
        if (!(settings.double_click_to_travel_to_quest
            && message->message_id == GW::UI::UIMessage::kMouseClick2
            && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost
            && GW::UI::BelongsToFrame(GW::UI::GetFrameByLabel(L"Quest"), GW::UI::GetFrameById(message->frame_id))) )
            return GW::Hook::LeaveHook();
        const auto packet = (GW::UI::UIPacket::kMouseAction*)wParam;
        if (!(packet->current_state == GW::UI::UIPacket::ActionState::MouseClick && (packet->child_offset_id & 0xffff0000) == 0x80000000)) 
            return GW::Hook::LeaveHook(); // Not a double click on a quest entry
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

    void OnQuestPathRecalculated(std::vector<GW::GamePos>& waypoints, void* args);
    void ClearCalculatedPath(GW::Constants::QuestID quest_id);
    struct CalculatedQuestPath;
    CalculatedQuestPath* GetCalculatedQuestPath(GW::Constants::QuestID quest_id, bool create_if_not_found = true);

    bool IsActiveQuestPath(const GW::Constants::QuestID quest_id)
    {
        const auto questlog = GW::QuestMgr::GetQuestLog();
        const auto active_quest = GW::QuestMgr::GetActiveQuest();
        if (!questlog || !active_quest)
            return false;
        if (quest_id == active_quest->quest_id)
            return true;
        if (!settings.show_paths_to_all_quests)
            return false;
        // De-duplicate other quests that are pointing to the same place!
        const auto quest = GW::QuestMgr::GetQuest(quest_id);
        if (!quest)
            return false; // Quest has just been removed?
        for (const auto q : *questlog) {
            if (quest->marker == q.marker) {
                return q.quest_id == quest_id;
            }
        }
        return false;
    }

    struct CalculatedQuestPath {
        CalculatedQuestPath(const GW::Constants::QuestID _quest_id)
            : quest_id(_quest_id) {}

        ~CalculatedQuestPath()
        {
            ClearMinimapLines();
        }

        CalculatedQuestPath(const CalculatedQuestPath&) = delete;
        CalculatedQuestPath& operator=(const CalculatedQuestPath&) = delete;
        CalculatedQuestPath(CalculatedQuestPath&&) = delete;
        CalculatedQuestPath& operator=(CalculatedQuestPath&&) = delete;

        std::vector<GW::GamePos> waypoints{};
        std::vector<CustomRenderer::CustomLine*> minimap_lines{};
        GW::GamePos previous_closest_waypoint{};
        GW::GamePos original_quest_marker{};
        GW::GamePos calculated_from{};
        GW::GamePos calculated_to{};
        clock_t calculated_at = 0;
        uint32_t current_waypoint = 0;
        GW::Constants::QuestID quest_id{};
        clock_t calculating = 0;
        // Cross-map routing: marker is on another physical map, routed via portals.
        // goal_world is the world-map goal; has_full_route is set once a whole route
        // has been plotted, after which moves only re-walk the current-map leg.
        bool is_cross_map = false;
        GW::Vec2f goal_world{};
        bool has_full_route = false;
        bool IsCalculating() {
            return calculating && TIMER_DIFF(calculating) < 5000;
        }

        void ClearMinimapLines()
        {
            for (const auto l : minimap_lines) {
                Minimap::Instance().custom_renderer.RemoveCustomLine(l);
            }
            minimap_lines.clear();
        }

        void DrawMinimapLines()
        {
            ClearMinimapLines();
            if (!(settings.draw_quest_path_on_terrain || settings.draw_quest_path_on_minimap || settings.draw_quest_path_on_mission_map))
                return;
            if (waypoints.empty())
                return;
            const size_t start_idx = current_waypoint > 0 ? current_waypoint - 1 : 0;
            bool first_line = true;
            bool past_break = false; // points before the first break are the current-map leg
            for (size_t i = start_idx; i < waypoints.size() - 1; i++) {
                // Cross-map routes separate maps with a break sentinel; don't draw across it.
                if (PathfindingWindow::IsRouteBreak(waypoints[i]) || PathfindingWindow::IsRouteBreak(waypoints[i + 1])) {
                    past_break = true;
                    continue;
                }
                const auto l = Minimap::Instance().custom_renderer.AddCustomLine(
                    waypoints[i], waypoints[i + 1],
                    std::format("{} - {}", static_cast<uint32_t>(quest_id), i).c_str(), true
                );
                l->from_player_pos = first_line; // anchor the leg's start to the player
                // Only the current-map leg sits at real positions — terrain-draw only it.
                l->draw_on_terrain = settings.draw_quest_path_on_terrain && !past_break;
                l->draw_on_minimap = settings.draw_quest_path_on_minimap;
                l->draw_on_mission_map = settings.draw_quest_path_on_mission_map;
                l->created_by_toolbox = true;
                l->color = QuestModule::GetQuestLineColor(quest_id);
                minimap_lines.push_back(l);
                first_line = false;
            }
            GameWorldRenderer::TriggerSyncAllMarkers();
        }

        [[nodiscard]] const GW::Quest* GetQuest() const
        {
            return GW::QuestMgr::GetQuest(quest_id);
        }

        [[nodiscard]] bool IsActive() const
        {
            const auto a = GW::QuestMgr::GetActiveQuestId() == quest_id;
            return a || (GetQuest() && Minimap::ShouldDrawAllQuests());
        }

        const GW::GamePos* CurrentWaypoint()
        {
            return &waypoints[current_waypoint];
        }

        const GW::GamePos* NextWaypoint()
        {
            return current_waypoint < waypoints.size() - 1 ? &waypoints[current_waypoint + 1] : nullptr;
        }

        void Recalculate(const GW::GamePos& from)
        {
            if (IsCalculating()) return;
            calculating = TIMER_INIT();

            // Cross-map marker: the route runs through portals on other maps. Compute it
            // via the pathfinding route API on a worker, then own the points here as the
            // quest path. Once a full route is plotted, moves only re-walk the current-map
            // leg (player -> exit portal) and reuse the rest.
            if (is_cross_map) {
                if (!PathfindingWindow::ReadyForPathing()) { calculating = 0; calculated_at = 0; return; }
                GW::Vec2f from_world{};
                WorldMapWidget::GamePosToWorldMap(from, from_world);
                const auto qid = quest_id;
                const auto gw = goal_world;
                const bool leg_only = has_full_route && PathfindingWindow::HasRouteForCurrentMap();
                Resources::EnqueueWorkerTask([qid, from, from_world, gw, leg_only] {
                    auto pts = new std::vector<GW::GamePos>();
                    bool ok = leg_only && PathfindingWindow::RecalculateRouteLeg(from, pts);
                    bool was_full = false;
                    if (!ok) { ok = PathfindingWindow::CalculateRoute(from_world, gw, pts); was_full = ok; }
                    Resources::EnqueueMainTask([qid, pts, ok, was_full] {
                        const auto cqp = GetCalculatedQuestPath(qid, false);
                        if (cqp && ok) {
                            if (was_full) cqp->has_full_route = true;
                            OnQuestPathRecalculated(*pts, (void*)qid);
                        }
                        else if (cqp) { cqp->calculating = 0; cqp->calculated_at = 0; }
                        delete pts;
                    });
                });
                calculated_from = from;
                return;
            }

            // If we're not calculating, and the starting point hasn't changed, and the destination hasn't changed, then we've already got our path
            if (calculated_at &&
                from == calculated_from && calculated_to == original_quest_marker) {
                OnQuestPathRecalculated(waypoints, (void*)quest_id);
                return;
            }

            // If quest marker has changed to infinity; clear any current markers
            if (original_quest_marker.x == INFINITY) {
                if (waypoints.size()) {
                    waypoints.clear();
                    OnQuestPathRecalculated(waypoints, (void*)quest_id); // No need to recalculate
                }
                return;
            }
            // Trigger a recalculation on another thread, and return the time that it started as calculating.
            calculating = PathfindingWindow::CalculatePath(from, original_quest_marker, OnQuestPathRecalculated, (void*)quest_id);
            if (calculating) {
                calculated_from = from;
                calculated_to = original_quest_marker;
            }
            else {
                // If calculating wasn't a valid timestamp, assume that it was rejected and reset for next loop
                calculated_at = 0;
            }
        }

        bool Update(const GW::GamePos& from)
        {
            static clock_t last_check = TIMER_INIT();
            if (TIMER_DIFF(last_check) < 33) return false;
            last_check = TIMER_INIT();

            if (IsCalculating()) {
                return false;
            }
            const auto quest = GetQuest();
            if (!quest) {
                ClearCalculatedPath(quest_id);
                return true;
            }

            if (!calculated_at) {
                Recalculate(from);
                return false;
            }
            constexpr float recalculate_when_moved_further_than = 100.f * 100.f;
            if (GetSquareDistance(from, calculated_from) > recalculate_when_moved_further_than) {
                Recalculate(from);
                return false;
            }
            // Cross-map routes span projected maps, so the nearest-waypoint progression
            // below (which assumes one coord space) is invalid — it would skip the leg.
            // The whole route is drawn from the start; the leg-recalc above keeps it fresh.
            if (is_cross_map) return false;
            const uint32_t original_waypoint = current_waypoint;

            const auto waypoint_len = waypoints.size();
            if (!waypoint_len)
                return false;

            const auto from_end_waypoint = GetSquareDistance(from, waypoints.back());
            // Find next waypoint
            current_waypoint = waypoint_len - 1;
            for (size_t i = 1; i < waypoint_len; i++) {
                if (GetSquareDistance(from, waypoints[i]) < from_end_waypoint) {
                    current_waypoint = i;
                    break;
                }
            }
            if (original_waypoint != current_waypoint) {
                calculated_from = from;
                UpdateUI();
            }
            return false;
        }

        void UpdateUI()
        {
            DrawMinimapLines();
            if (waypoints.empty())
                return;

            const auto& current_waypoint_pos = waypoints[current_waypoint];
            const auto waypoint_distance = GetSquareDistance(current_waypoint_pos, previous_closest_waypoint);
            constexpr float update_when_waypoint_changed_more_than = 300.f * 300.f;
            if (IsActive() &&
                waypoint_distance > update_when_waypoint_changed_more_than) {
                previous_closest_waypoint = waypoints[current_waypoint];
            }
        }
    };
    void BlockQuestSound()
    {
        AudioSettings::BlockSoundForMs(L"\xe14d\x0101", 1000);
        AudioSettings::BlockSoundForMs(L"\xe14c\x0101", 1000);
    }


    // Custom quests have ids greater than the count in-game - there are some assertions that don't like this!
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
        if (found == calculated_quest_paths.end())
            return;
        auto cqp = found->second;
        calculated_quest_paths.erase(found);
        delete cqp;
    }

    CalculatedQuestPath* GetCalculatedQuestPath(GW::Constants::QuestID quest_id, bool create_if_not_found = true)
    {
        const auto found = calculated_quest_paths.find(quest_id);
        if (found != calculated_quest_paths.end()) return found->second;
        if (!create_if_not_found)
            return nullptr;
        const auto cqp = new CalculatedQuestPath(quest_id);
        calculated_quest_paths[quest_id] = cqp;
        return cqp;
    }

    bool is_spoofing_quest_update = false;

    // Settings
    GW::GamePos* GetPlayerPos()
    {
        const auto p = GW::Agents::GetControlledCharacter();
        return p ? &p->pos : nullptr;
    }

    // Cast helper
    float GetSquareDistance(const GW::GamePos& a, const GW::GamePos& b)
    {
        return GetSquareDistance(static_cast<GW::Vec2f>(a), static_cast<GW::Vec2f>(b));
    }

    // Called by PathfindingWindow when a path has been calculated. Should be on the main loop.
    void OnQuestPathRecalculated(std::vector<GW::GamePos>& waypoints, void* args)
    {
        const auto cqp = GetCalculatedQuestPath(*reinterpret_cast<GW::Constants::QuestID*>(&args), false);
        if (!cqp)
            return;
        cqp->current_waypoint = 0;
        cqp->waypoints = std::move(waypoints); // Move

        // Cross-map routes mix projected coord spaces (and start at the player), so the
        // flip + nearest-waypoint heuristic doesn't apply — draw from waypoint 0.
        const auto waypoint_len = cqp->waypoints.size();
        if (waypoint_len && !cqp->is_cross_map) {
            if (GetSquareDistance(cqp->waypoints.back(), cqp->calculated_from) < GetSquareDistance(cqp->waypoints.front(), cqp->calculated_from)) {
                // Waypoint array is in descending distance, flip it
                std::ranges::reverse(cqp->waypoints);
            }
            const auto from_end_waypoint = GetSquareDistance(cqp->calculated_from, cqp->waypoints.back());
            // Find next waypoint
            cqp->current_waypoint = waypoint_len - 1;
            for (size_t i = 1; i < waypoint_len; i++) {
                if (GetSquareDistance(cqp->calculated_from, cqp->waypoints[i]) < from_end_waypoint) {
                    cqp->current_waypoint = i;
                    break;
                }
            }
        }
        cqp->calculated_at = TIMER_INIT();
        cqp->calculating = 0;
        cqp->UpdateUI();
    }

    void RefreshQuestPath(GW::Constants::QuestID quest_id)
    {
        GW::GameThread::Enqueue([quest_id] {
            if (!IsActiveQuestPath(quest_id)) {
                ClearCalculatedPath(quest_id);
                return;
            }
            const auto quest = GW::QuestMgr::GetQuest(quest_id);
            const auto pos = quest ? GetPlayerPos() : nullptr;
            if (!pos)
                return;
            const auto cqp = GetCalculatedQuestPath(quest_id);
            if (!cqp)
                return;
            cqp->original_quest_marker = quest->marker;
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
                    quest->log_state |= 1; // Avoid asking for description about this quest
                }
            } break;
            case GW::UI::UIMessage::kStartMapLoad: {
                const auto q = GW::QuestMgr::GetActiveQuestId();
                if (q != GW::Constants::QuestID::None) quest_id_before_map_load = q;
            } break;
            case GW::UI::UIMessage::kSendSetActiveQuest: {
                const auto quest_id = static_cast<GW::Constants::QuestID>((uint32_t)wparam);
                if (setting_custom_quest_marker) {
                    // This triggers if the player has no quests, or the map has just loaded; we want to "undo" this by spoofing the previous quest selection if there is one
                    status->blocked = true;
                    QuestModule::SetActiveQuestId(GW::QuestMgr::GetActiveQuestId(), false);
                    return;
                }
                player_chosen_quest_id = quest_id;
                if (quest_id == custom_quest_id) {
                    // If the player has chosen the custom quest, spoof the response without asking the server
                    status->blocked = true;
                    QuestModule::SetActiveQuestId(quest_id, false);
                }
            }
            break;
            case GW::UI::UIMessage::kSendAbandonQuest: {
                const auto quest_id = static_cast<GW::Constants::QuestID>((uint32_t)wparam);
                if (quest_id == custom_quest_id) {
                    status->blocked = true;
                    QuestModule::SetCustomQuestMarker({0, 0});
                }
            }
            break;
            case GW::UI::UIMessage::kOnScreenMessage: {
                // Block the on-screen message when the custom marker is placed
                if (setting_custom_quest_marker) {
                    status->blocked = true;
                }
            }
            break;
        }
    }

    // Callback invoked by quest related ui messages. All messages sent should have the quest id as first wparam variable
    void OnPostUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* packet, void*)
    {
        if (status->blocked)
            return;
        switch (message_id) {
            case GW::UI::UIMessage::kQuestDetailsChanged:
            case GW::UI::UIMessage::kQuestAdded:
            case GW::UI::UIMessage::kClientActiveQuestChanged: {
                const auto quest = GW::QuestMgr::GetQuest(*(GW::Constants::QuestID*)packet);
                if (quest && settings.keep_current_quest_when_new_quest_added && quest->quest_id != player_chosen_quest_id && GW::QuestMgr::GetQuest(player_chosen_quest_id)) {
                    // Quest assigned without user interaction
                    QuestModule::SetActiveQuestId(player_chosen_quest_id, true);
                }
                RefreshQuestPath(*static_cast<GW::Constants::QuestID*>(packet));
            } break;
            case GW::UI::UIMessage::kMapLoaded:
                BlockQuestSound();
                break;
        }
    }
    bool was_loading = true;



    void OnMapLoaded() {
        if (GW::UI::IsLoadingScreenShown()) return;
        BlockQuestSound();
        QuestModule::FetchMissingQuestInfo();
        ClearCalculatedQuestPaths();
        if (custom_quest_marker_world_pos.y != 0 || custom_quest_marker_world_pos.x != 0) {
            QuestModule::SetCustomQuestMarker(custom_quest_marker_world_pos, quest_id_before_map_load == custom_quest_id);
        }
        RefreshAllQuestPaths();
    }

    bool refresh_all_quest_paths_queued = 0;

    // Dangerously frees gw memory!
    void RemoveQuest(GW::Constants::QuestID quest_id)
    {
        auto* quest = GW::QuestMgr::GetQuest(quest_id);
        if (!quest) return;
        auto w = GW::GetWorldContext();
        auto& quest_log = w->quest_log;
        const auto idx = quest - quest_log.m_buffer;
        const auto remaining = quest_log.m_size - idx - 1;
        if (remaining > 0) memmove(quest, quest + 1, remaining * sizeof(*quest_log.m_buffer));
        if (w->active_quest_id == quest_id) {
            w->active_quest_id = GW::Constants::QuestID::None;
        }
        quest_log.m_size--;
        auto* removed = &quest_log.m_buffer[quest_log.m_size];
        GW::MemoryMgr::MemFree(removed->objectives);
        GW::MemoryMgr::MemFree(removed->description);
        GW::MemoryMgr::MemFree(removed->npc);
        GW::MemoryMgr::MemFree(removed->name);
        GW::MemoryMgr::MemFree(removed->location);

        GW::UI::SendUIMessage(GW::UI::UIMessage::kMessage_0x10000152, (void*)&quest_id);
    }

    // Dangerously allocated gw memory!
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
void QuestModule::SetCustomQuestMarker(const GW::Vec2f& world_pos, bool set_active)
{
    BlockQuestSound();
    Instance().Initialize();
    if (!GW::Agents::GetControlledCharacter()) return; // Map not ready

    custom_quest_marker_world_pos = world_pos;

    RemoveQuest(custom_quest_id);

    if (custom_quest_marker_world_pos.x == 0 && custom_quest_marker_world_pos.y == 0) {
        PathfindingWindow::ClearRoute();
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
        L"\x452", // "Map Travel"
        L"\x564", // "Primary Quests"
        L"\x108\x107You've placed a custom marker on the map.\x1"
    );
    ASSERT(quest);
    if (set_active) {
        QuestModule::SetActiveQuestId(quest->quest_id, false);
    }

    // When the marker is on a different physical map, the quest path is a cross-map
    // route via portals (computed + recalculated through the route API, owned by the
    // CalculatedQuestPath). On the same map it's an ordinary in-map quest path. Either
    // way QuestModule owns and draws it; tag the path so Recalculate picks the route.
    const auto cur_map_id = GW::Map::GetMapID();
    const auto fh_cur = PathfindingWindow::GetMapFileId(cur_map_id);
    const auto fh_dst = map_to == GW::Constants::MapID::Count ? 0u : PathfindingWindow::GetMapFileId(map_to);
    const bool cross_map = fh_cur && fh_dst && fh_cur != fh_dst;
    if (auto* cqp = GetCalculatedQuestPath(custom_quest_id)) {
        cqp->is_cross_map = cross_map;
        cqp->goal_world = custom_quest_marker_world_pos;
        cqp->has_full_route = false; // new marker → plot the whole route first
    }
    if (!cross_map) PathfindingWindow::ClearRoute();

    setting_custom_quest_marker = false;
    RefreshQuestPath(custom_quest_id);
    for (const auto& cb : custom_marker_callbacks)
        cb();
}

void QuestModule::ClearCustomQuestMarker()
{
    SetCustomQuestMarker({0, 0});
}

void QuestModule::AddCustomMarkerChangedCallback(CustomMarkerChangedCallback cb) { custom_marker_callbacks.push_back(cb); }
void QuestModule::RemoveCustomMarkerChangedCallback(CustomMarkerChangedCallback cb) { std::erase(custom_marker_callbacks, cb); }

std::vector<QuestObjective> QuestModule::ParseQuestObjectives(GW::Constants::QuestID quest_id)
{
    const auto quest = GW::QuestMgr::GetQuest(quest_id);
    std::vector<QuestObjective> out;
    if (!quest) return out;
    const wchar_t* next_objective_enc = nullptr;
    const wchar_t* current_objective_enc = quest->objectives;
    if (!quest->objectives) {
        if (quest_id == custom_quest_id)
            return out;
        BlockQuestSound();
        GW::QuestMgr::RequestQuestInfo(quest);
    }
        
    while (current_objective_enc) {
        next_objective_enc = wcschr(current_objective_enc, 0x2);
        size_t current_objective_len = next_objective_enc ? next_objective_enc - current_objective_enc : wcslen(current_objective_enc);

        auto enc_str = std::wstring(current_objective_enc, current_objective_len);
        auto content_start = enc_str.find(0x10a);
        if (content_start == std::wstring::npos)
            break;
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
    ImGui::CheckboxWithHelp("Keep current quest when accepting a new one", &settings.keep_current_quest_when_new_quest_added,
        "By default, Guild Wars changes your currently selected quest to the one you've just taken from an NPC.\nThis can be annoying if you don't realise your quest marker is now taking you somewhere different!\nTick this to make sure your current quest isn't changed when a new quest is added to your log."
    );
    ImGui::Checkbox("Double click a quest in the quest log window to travel to nearest outpost", &settings.double_click_to_travel_to_quest);
    ImGui::Text("Draw path to quest marker on:");
    bool recalc_quest_paths = false;
    recalc_quest_paths |= ImGui::Checkbox("Terrain##terrianquestpath", &settings.draw_quest_path_on_terrain);
    recalc_quest_paths |= ImGui::Checkbox("Minimap##minimapquestpath", &settings.draw_quest_path_on_minimap);
    recalc_quest_paths |= ImGui::Checkbox("Mission Map##missionmapquestpath", &settings.draw_quest_path_on_mission_map);
    ImGui::Checkbox("World Map##worldmapquestpath", &WorldMapWidget::ShowLinesOnWorldMap());
#ifdef _DEBUG
    recalc_quest_paths |= ImGui::Checkbox("Show paths to all quests##drawallquestpaths", &settings.show_paths_to_all_quests);
#endif
    if(recalc_quest_paths)
        RefreshAllQuestPaths();
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
    if (initialised)
        return;
    initialised = true;
    ToolboxModule::Initialize();
    SettingsRegistry::Register(this, settings);

    memset(&custom_quest_marker, 0, sizeof(custom_quest_marker));
    uintptr_t address = GW::Scanner::FindAssertion("UiCtlWebLink.cpp", "challengeId < CHALLENGES", 0, -0x7);
    if (address) {
        bypass_custom_quest_assertion_patch.SetPatch(address, "\xeb", 1);
        bypass_custom_quest_assertion_patch.TogglePatch(true);
    }

    constexpr GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kQuestDetailsChanged,
        GW::UI::UIMessage::kQuestAdded,
        GW::UI::UIMessage::kClientActiveQuestChanged,
        GW::UI::UIMessage::kServerActiveQuestChanged,
        GW::UI::UIMessage::kMapLoaded,
        GW::UI::UIMessage::kOnScreenMessage,
        GW::UI::UIMessage::kSendSetActiveQuest,
        GW::UI::UIMessage::kSendAbandonQuest,
        GW::UI::UIMessage::kStartMapLoad
    };
    for (const auto ui_message : ui_messages) {
        // Post callbacks, non blocking
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
    if (quest_id == custom_quest_id)
        notify_server = false;
    
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
    if (bypass_custom_quest_assertion_patch.IsValid())
        bypass_custom_quest_assertion_patch.Reset();
}

void QuestModule::Update(float)
{
    const auto pos = GetPlayerPos();
    if (!pos) {
        was_loading = true;
        return;
    }
    if (was_loading) {
        if (GW::UI::IsLoadingScreenShown())
            return;
        OnMapLoaded();
        was_loading = false;
    }
    if (fetch_missing_quest_info_queued) {
        // NB: We only do this once the loading splash screen is gone
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

QuestObjective::QuestObjective(const GW::Constants::QuestID quest_id, const wchar_t* objective_enc, const bool is_completed)
    : quest_id(quest_id),
      objective_enc(std::make_unique<GuiUtils::EncString>(objective_enc)),
      is_completed(is_completed) {}
