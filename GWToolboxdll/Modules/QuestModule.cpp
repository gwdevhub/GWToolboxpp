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
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Utilities/MemoryPatcher.h>
#include <Utils/ToolboxUtils.h>
#include "AudioSettings.h"

namespace {
    bool draw_quest_path_on_terrain = false;
    bool draw_quest_path_on_minimap = true;
    bool draw_quest_path_on_mission_map = true;
    bool show_paths_to_all_quests = false;
    GW::HookEntry pre_ui_message_entry;
    GW::HookEntry post_ui_message_entry;
    bool initialised = false;

    clock_t last_fetched_missing_quest_info = 0;

    bool double_click_to_travel_to_quest = true;

    GW::Constants::QuestID custom_quest_id = static_cast<GW::Constants::QuestID>(0x0000fdd);
    GW::Quest custom_quest_marker;
    GW::Vec2f custom_quest_marker_world_pos;
    GW::Constants::QuestID player_chosen_quest_id = GW::Constants::QuestID::None;
    bool setting_custom_quest_marker = false;

    const wchar_t* custom_quest_marker_enc_name = L"\x452"; // "Map Travel"

    clock_t last_quest_clicked = 0;

    GW::UI::UIInteractionCallback QuestLogRow_UICallback_Func = nullptr, QuestLogRow_UICallback_Ret = nullptr;

    // If double clicked on a quest entry, teleport to nearest outpost
    void OnQuestLogRow_UICallback(GW::UI::InteractionMessage* message, void* wParam, void* lParam)
    {
        GW::Hook::EnterHook();
        QuestLogRow_UICallback_Ret(message, wParam, lParam);
        if (!(double_click_to_travel_to_quest
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

    bool IsActiveQuestPath(const GW::Constants::QuestID quest_id)
    {
        const auto questlog = GW::QuestMgr::GetQuestLog();
        const auto active_quest = GW::QuestMgr::GetActiveQuest();
        if (!questlog || !active_quest)
            return false;
        if (quest_id == active_quest->quest_id)
            return true;
        if (!show_paths_to_all_quests)
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
            if (!(draw_quest_path_on_terrain || draw_quest_path_on_minimap || draw_quest_path_on_mission_map))
                return;
            if (waypoints.empty())
                return;
            const size_t start_idx = current_waypoint > 0 ? current_waypoint - 1 : 0;
            for (size_t i = start_idx; i < waypoints.size() - 1; i++) {
                const auto l = Minimap::Instance().custom_renderer.AddCustomLine(
                    waypoints[i], waypoints[i + 1],
                    std::format("{} - {}", static_cast<uint32_t>(quest_id), i).c_str(), true
                );
                l->from_player_pos = i == start_idx;
                l->draw_on_terrain = draw_quest_path_on_terrain;
                l->draw_on_minimap = draw_quest_path_on_minimap;
                l->draw_on_mission_map = draw_quest_path_on_mission_map;
                l->created_by_toolbox = true;
                l->color = QuestModule::GetQuestLineColor(quest_id);
                minimap_lines.push_back(l);
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
            if (calculated_at &&
                from == calculated_from && calculated_to == original_quest_marker) {
                calculating = TIMER_INIT();
                OnQuestPathRecalculated(waypoints, (void*)quest_id); // No need to recalculate
                return;
            }
            calculated_from = from;
            calculated_to = original_quest_marker;
            if (original_quest_marker.x == INFINITY) {
                if (waypoints.size()) {
                    // Quest marker has changed to infinity; clear any current markers
                    waypoints.clear();
                    calculating = TIMER_INIT();
                    OnQuestPathRecalculated(waypoints, (void*)quest_id); // No need to recalculate
                }
                return;
            }
            calculating = PathfindingWindow::CalculatePath(calculated_from, calculated_to, OnQuestPathRecalculated, (void*)quest_id);
            if (!IsCalculating()) {
                calculated_at = 0;
            }
        }

        bool Update(const GW::GamePos& from)
        {
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

        const auto waypoint_len = cqp->waypoints.size();
        if (waypoint_len) {
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
                const auto quest_id = *(GW::Constants::QuestID*)wparam;
                if (quest_id == custom_quest_id) {
                    GW::QuestMgr::GetQuest(quest_id)->log_state |= 1; // Avoid asking for description about this quest
                }
            }
            break;
            case GW::UI::UIMessage::kStartMapLoad:
                quest_id_before_map_load = GW::QuestMgr::GetActiveQuestId();
                break;
            case GW::UI::UIMessage::kSendSetActiveQuest: {
                const auto quest_id = static_cast<GW::Constants::QuestID>((uint32_t)wparam);
                if (setting_custom_quest_marker) {
                    // This triggers if the player has no quests, or the map has just loaded; we want to "undo" this by spoofing the previous quest selection if there is one
                    status->blocked = true;
                    QuestModule::EmulateQuestSelected(GW::QuestMgr::GetActiveQuestId());
                    return;
                }
                player_chosen_quest_id = quest_id;
                if (quest_id == custom_quest_id) {
                    // If the player has chosen the custom quest, spoof the response without asking the server
                    status->blocked = true;
                    QuestModule::EmulateQuestSelected(quest_id);
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
            case GW::UI::UIMessage::kClientActiveQuestChanged:
                RefreshQuestPath(*static_cast<GW::Constants::QuestID*>(packet));
                break;
            case GW::UI::UIMessage::kMapLoaded:
                ClearCalculatedQuestPaths();
                if (custom_quest_marker.quest_id != (GW::Constants::QuestID)0) {
                    QuestModule::SetCustomQuestMarker(custom_quest_marker_world_pos, player_chosen_quest_id == custom_quest_marker.quest_id);
                    if (quest_id_before_map_load == custom_quest_marker.quest_id)
                        GW::QuestMgr::SetActiveQuestId(quest_id_before_map_load);
                }
                RefreshAllQuestPaths();
                break;
        }
    }
} // namespace

const GW::Quest* QuestModule::GetCustomQuestMarker()
{
    return GW::QuestMgr::GetQuest(custom_quest_id);
}

void QuestModule::SetCustomQuestMarker(const GW::Vec2f& world_pos, bool set_active)
{
    Instance().Initialize();
    if (!GW::Agents::GetControlledCharacter())
        return; // Map not ready
    custom_quest_marker_world_pos = world_pos;
    if (GW::QuestMgr::GetQuest(custom_quest_id)) {
        struct QuestRemovePacket : GW::Packet::StoC::PacketBase {
            GW::Constants::QuestID quest_id = custom_quest_id;
        };
        QuestRemovePacket quest_remove_packet;
        quest_remove_packet.header = GAME_SMSG_QUEST_REMOVE;
        GW::StoC::EmulatePacket(&quest_remove_packet);
        memset(&custom_quest_marker, 0, sizeof(custom_quest_marker));
    }
    if (custom_quest_marker_world_pos.x == 0 && custom_quest_marker_world_pos.y == 0)
        return;

    setting_custom_quest_marker = true;

    const auto map_id = WorldMapWidget::GetMapIdForLocation(custom_quest_marker_world_pos);

    struct QuestInfoPacket : GW::Packet::StoC::PacketBase {
        GW::Constants::QuestID quest_id = custom_quest_id;
        uint32_t log_state = 0x20;
        wchar_t location[8]{};
        wchar_t name[8]{};
        wchar_t npc[8]{};
        GW::Constants::MapID map_from;
    };

    QuestInfoPacket quest_add_packet;
    quest_add_packet.header = GAME_SMSG_QUEST_GENERAL_INFO;
    quest_add_packet.quest_id = custom_quest_id;
    wcscpy(quest_add_packet.location, L"\x564"); // Primary Quests
    wcscpy(quest_add_packet.name, custom_quest_marker_enc_name);
    quest_add_packet.map_from = GW::Constants::MapID::Count;
    GW::StoC::EmulatePacket(&quest_add_packet);

    struct QuestDescriptionPacket : GW::Packet::StoC::PacketBase {
        GW::Constants::QuestID quest_id = custom_quest_id;
        wchar_t description[128] = {0};
        wchar_t objective[128] = {0};
    };
    QuestDescriptionPacket quest_description_packet;
    quest_description_packet.header = GAME_SMSG_QUEST_DESCRIPTION;

    swprintf(quest_description_packet.description, _countof(quest_description_packet.description),
             L"\x108\x107You've placed a custom marker on the map.\x1");

    GW::StoC::EmulatePacket(&quest_description_packet);

    struct QuestMarkerPacket : GW::Packet::StoC::PacketBase {
        GW::Constants::QuestID quest_id = custom_quest_id;
        GW::GamePos marker;
        GW::Constants::MapID map_id;
    };
    QuestMarkerPacket quest_marker_packet;
    quest_marker_packet.header = GAME_SMSG_QUEST_UPDATE_MARKER;
    quest_marker_packet.marker = {INFINITY, INFINITY};
    quest_marker_packet.map_id = map_id;
    if (map_id == GW::Map::GetMapID())
        WorldMapWidget::WorldMapToGamePos(custom_quest_marker_world_pos, quest_marker_packet.marker);
    GW::StoC::EmulatePacket(&quest_marker_packet);

    setting_custom_quest_marker = false;

    const auto quest = GetCustomQuestMarker();
    ASSERT(quest);
    custom_quest_marker = *quest;
    if (set_active)
        GW::QuestMgr::SetActiveQuestId(custom_quest_id);
    RefreshQuestPath(custom_quest_id);
}

std::vector<QuestObjective> QuestModule::ParseQuestObjectives(GW::Constants::QuestID quest_id)
{
    const auto quest = GW::QuestMgr::GetQuest(quest_id);
    std::vector<QuestObjective> out;
    if (!quest) return out;
    const wchar_t* next_objective_enc = nullptr;
    const wchar_t* current_objective_enc = quest->objectives;
    if (!quest->objectives)
        GW::QuestMgr::RequestQuestInfo(quest);
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
    ImGui::Checkbox("Double click a quest in the quest log window to travel to nearest outpost", &double_click_to_travel_to_quest);
    ImGui::Text("Draw path to quest marker on:");
    bool recalc_quest_paths = false;
    recalc_quest_paths |= ImGui::Checkbox("Terrain##terrianquestpath", &draw_quest_path_on_terrain);
    recalc_quest_paths |= ImGui::Checkbox("Minimap##minimapquestpath", &draw_quest_path_on_minimap);
    recalc_quest_paths |= ImGui::Checkbox("Mission Map##missionmapquestpath", &draw_quest_path_on_mission_map);
#ifdef _DEBUG
    recalc_quest_paths |= ImGui::Checkbox("Show paths to all quests##drawallquestpaths", &show_paths_to_all_quests);
#endif
    if(recalc_quest_paths)
        RefreshAllQuestPaths();
    ImGui::ShowHelp("The higher this value, the more accurate the path will be, but the more CPU it will use.");
}

void QuestModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
    LOAD_BOOL(draw_quest_path_on_minimap);
    LOAD_BOOL(draw_quest_path_on_mission_map);
    LOAD_BOOL(draw_quest_path_on_terrain);
    LOAD_BOOL(show_paths_to_all_quests);
    using namespace Pathing;
    float custom_quest_marker_world_pos_x = .0f;
    float custom_quest_marker_world_pos_y = .0f;
    LOAD_FLOAT(custom_quest_marker_world_pos_x);
    LOAD_FLOAT(custom_quest_marker_world_pos_y);
    LOAD_BOOL(double_click_to_travel_to_quest);
    custom_quest_marker_world_pos = {custom_quest_marker_world_pos_x, custom_quest_marker_world_pos_y};
    GW::GameThread::Enqueue([] {
        SetCustomQuestMarker(custom_quest_marker_world_pos);
    });
}

void QuestModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(draw_quest_path_on_minimap);
    SAVE_BOOL(draw_quest_path_on_mission_map);
    SAVE_BOOL(draw_quest_path_on_terrain);
    SAVE_BOOL(show_paths_to_all_quests);
    using namespace Pathing;
    float custom_quest_marker_world_pos_x = custom_quest_marker_world_pos.x;
    float custom_quest_marker_world_pos_y = custom_quest_marker_world_pos.y;
    SAVE_FLOAT(custom_quest_marker_world_pos_x);
    SAVE_FLOAT(custom_quest_marker_world_pos_y);
    SAVE_BOOL(double_click_to_travel_to_quest);
}

void QuestModule::Initialize()
{
    if (initialised)
        return;
    initialised = true;
    ToolboxModule::Initialize();

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
        GW::UI::RegisterUIMessageCallback(&pre_ui_message_entry, ui_message, OnPreUIMessage, -0x4000);
        GW::UI::RegisterUIMessageCallback(&post_ui_message_entry, ui_message, OnPostUIMessage, 0x4000);
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
    RefreshAllQuestPaths();
}

void QuestModule::EmulateQuestSelected(GW::Constants::QuestID quest_id)
{
    Instance().Initialize();
    const auto quest = GW::QuestMgr::GetQuest(quest_id);
    if (!quest)
        return;
    GW::UI::UIPacket::kServerActiveQuestChanged packet = {
        .quest_id = quest->quest_id,
        .marker = quest->marker,
        .h0024 = quest->h0024,
        .map_id = quest->map_to,
        .log_state = quest->log_state
    };
    GW::UI::SendUIMessage(GW::UI::UIMessage::kClientActiveQuestChanged, &packet);
    GW::GetWorldContext()->active_quest_id = quest->quest_id;
    GW::UI::SendUIMessage(GW::UI::UIMessage::kServerActiveQuestChanged, &packet);
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
    if (!pos)
        return;
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
    if (TIMER_DIFF(last_fetched_missing_quest_info) < 250) return;
    last_fetched_missing_quest_info = TIMER_INIT();
    
    GW::GameThread::Enqueue([] {
        bool requested = false;
        const auto quest_log = GW::QuestMgr::GetQuestLog();
        if (!quest_log) return;
        for (auto& quest : *quest_log) {
            if ((quest.log_state & 1) == 0) {
                GW::QuestMgr::RequestQuestInfo(&quest, true);
                requested = true;
            }
        }
        if (requested) {
            // Block quest change sound that this will trigger.
            AudioSettings::BlockSoundForMs(L"\xe14c\x0101", 500);
        }
    });
}

void QuestModule::Terminate()
{
    ToolboxModule::Terminate();
    initialised = false;
}

QuestObjective::QuestObjective(const GW::Constants::QuestID quest_id, const wchar_t* objective_enc, const bool is_completed)
    : quest_id(quest_id),
      objective_enc(new GuiUtils::EncString(objective_enc)),
      is_completed(is_completed) {}

QuestObjective::~QuestObjective()
{
    if (objective_enc)
        objective_enc->Release();
}
