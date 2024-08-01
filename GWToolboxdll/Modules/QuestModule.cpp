#include <GWCA/Utilities/Hook.h>

#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include "QuestModule.h"

#include <Windows/Pathfinding/PathfindingWindow.h>
#include <Widgets/Minimap/CustomRenderer.h>
#include <Widgets/Minimap/Minimap.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Utils/GuiUtils.h>
#include "Resources.h"

namespace {
    constexpr auto quest_colors = std::to_array<Color>({
        Colors::Red(), Colors::Pink(), Colors::Purple(),
        Colors::DeepPurple(), Colors::Indigo(), Colors::Blue(),
        Colors::LightBlue(), Colors::Cyan(), Colors::Teal(),
        Colors::Yellow(), Colors::Amber(), Colors::Orange(),
        Colors::DeepOrange(), Colors::Green(), Colors::Lime(),
        Colors::Magenta(), Colors::Gold(), Colors::BlueGrey(),
        Colors::White(), Colors::MaterialBlue(), Colors::MaterialGreen(),
        Colors::MaterialRed(), Colors::MaterialYellow()
    });

    bool draw_quest_path_on_terrain = true;
    bool draw_quest_path_on_minimap = true;
    GW::HookEntry ui_message_entry;

    bool waiting_for_pathing = false;

    void OnQuestPathRecalculated(std::vector<GW::GamePos>& waypoints, void* args);
    void ClearCalculatedPath(GW::Constants::QuestID quest_id);
    bool IsActiveQuestPath(GW::Constants::QuestID quest_id)
    {
        const auto questlog = GW::QuestMgr::GetQuestLog();
        const auto active_quest = GW::QuestMgr::GetActiveQuest();
        if (!questlog || !active_quest)
            return false;
        if (quest_id == active_quest->quest_id)
            return true;
        if (!Minimap::ShouldDrawAllQuests())
            return false;
        const auto quest = GW::QuestMgr::GetQuest(quest_id);
        auto lowest_quest_id_by_position = quest_id;
        for (const auto q : *questlog) {
            if (quest->marker == q.marker) {
                lowest_quest_id_by_position = std::min(lowest_quest_id_by_position, q.quest_id);
                break;
            }
        }
        if (quest_id != lowest_quest_id_by_position) {
            return false;
        }
        return true;
    }

    struct CalculatedQuestPath {
        CalculatedQuestPath(GW::Constants::QuestID _quest_id)
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
        bool calculating = false;

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
            if (!draw_quest_path_on_terrain && !draw_quest_path_on_minimap)
                return;
            for (size_t i = current_waypoint > 0 ? current_waypoint - 1 : 0; i < waypoints.size() - 1; i++) {
                const auto l = Minimap::Instance().custom_renderer.AddCustomLine(
                    waypoints[i], waypoints[i + 1],
                    std::format("{} - {}", (uint32_t)quest_id, i).c_str(), true
                );
                l->draw_on_terrain = draw_quest_path_on_terrain;
                l->created_by_toolbox = true;
                l->color = QuestModule::GetQuestColor(quest_id);
                minimap_lines.push_back(l);
            }
        }

        const GW::Quest* GetQuest()
        {
            return GW::QuestMgr::GetQuest(quest_id);
        }

        bool IsActive()
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
            if (from == calculated_from && calculated_to == original_quest_marker) {
                calculating = true;
                OnQuestPathRecalculated(waypoints, (void*)quest_id); // No need to recalculate
                return;
            }
            calculated_from = from;
            calculated_to = original_quest_marker;
            if (original_quest_marker.x == INFINITY)
                return;
            calculating = PathfindingWindow::CalculatePath(calculated_from, calculated_to, OnQuestPathRecalculated, (void*)quest_id);
        }

        bool Update(const GW::GamePos& from)
        {
            if (calculating) {
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
            uint32_t original_waypoint = current_waypoint;

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
            if (waypoints.empty())
                return;
            DrawMinimapLines();
            const auto& current_waypoint_pos = waypoints[current_waypoint];
            const auto waypoint_distance = GetSquareDistance(current_waypoint_pos, previous_closest_waypoint);
            constexpr float update_when_waypoint_changed_more_than = 300.f * 300.f;
            if (IsActive() &&
                waypoint_distance > update_when_waypoint_changed_more_than) {
                previous_closest_waypoint = waypoints[current_waypoint];
            }
        }
    };

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
        auto cqp = new CalculatedQuestPath(quest_id);
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

        if (!cqp->waypoints.empty() && GetSquareDistance(cqp->waypoints.back(), cqp->calculated_from) < GetSquareDistance(cqp->waypoints.front(), cqp->calculated_from)) {
            // Waypoint array is in descending distance, flip it
            std::ranges::reverse(cqp->waypoints);
        }

        const auto waypoint_len = cqp->waypoints.size();
        if (!waypoint_len) {
            cqp->calculating = false;
            cqp->calculated_at = TIMER_INIT();
            return;
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

        cqp->calculated_at = TIMER_INIT();
        cqp->calculating = false;
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
            auto cqp = GetCalculatedQuestPath(quest_id);
            if (!cqp)
                return;
            cqp->original_quest_marker = quest->marker;
            cqp->Recalculate(*pos);
        });
    }

    // Callback invoked by quest related ui messages. All messages sent should have the quest id as first wparam variable
    void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* packet, void*)
    {
        switch (message_id) {
            case GW::UI::UIMessage::kQuestDetailsChanged:
            case GW::UI::UIMessage::kQuestAdded:
            case GW::UI::UIMessage::kClientActiveQuestChanged:
                RefreshQuestPath(*static_cast<GW::Constants::QuestID*>(packet));
                break;
            case GW::UI::UIMessage::kMapLoaded:
            default:
                for (auto quest_path : calculated_quest_paths | std::views::values) {
                    delete quest_path;
                }
                calculated_quest_paths.clear();
                RefreshQuestPath(GW::QuestMgr::GetActiveQuestId());
                break;
        }
    }
} // namespace

std::vector<QuestObjective> QuestModule::ParseQuestObjectives(GW::Constants::QuestID quest_id)
{
    const auto quest = GW::QuestMgr::GetQuest(quest_id);
    std::vector<QuestObjective> out;
    if (!quest) return out;
    const wchar_t* next_objective_enc = nullptr;
    const wchar_t* current_objective_enc = quest->objectives;
    while (current_objective_enc) {
        next_objective_enc = wcschr(current_objective_enc, 0x2);
        size_t current_objective_len = next_objective_enc ? next_objective_enc - current_objective_enc : wcslen(current_objective_enc);

        auto enc_str = std::wstring(current_objective_enc, current_objective_len);
        auto content_start = enc_str.find(0x10a);
        if (!content_start)
            break;
        //ASSERT(content_start != std::wstring::npos);
        content_start++;

        enc_str = enc_str.substr(content_start, enc_str.size() - content_start - 1);

        const auto is_complete = *current_objective_enc == 0x2af5;

        out.push_back({quest_id, enc_str.c_str(), is_complete});

        current_objective_enc = next_objective_enc ? next_objective_enc + 1 : nullptr;
    }
    return out;
}

ImU32 QuestModule::GetQuestColor(GW::Constants::QuestID quest_id)
{
    const auto is_active_quest = GW::QuestMgr::GetActiveQuestId() == quest_id;
    if (is_active_quest) {
        return Minimap::Instance().symbols_renderer.color_quest;
    }
    const auto quest_index = (uint32_t)quest_id % quest_colors.size();
    return quest_colors.at(quest_index);
}

void QuestModule::DrawSettingsInternal()
{
    ImGui::Text("Draw path to quest marker on:");
    ImGui::Checkbox("Terrain##drawquestpath", &draw_quest_path_on_terrain);
    ImGui::Checkbox("Minimap##drawquestpath", &draw_quest_path_on_minimap);
}

void QuestModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
    LOAD_BOOL(draw_quest_path_on_minimap);
    LOAD_BOOL(draw_quest_path_on_terrain);
}

void QuestModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(draw_quest_path_on_minimap);
    SAVE_BOOL(draw_quest_path_on_terrain);
}

void QuestModule::Initialize()
{
    ToolboxModule::Initialize();

    constexpr auto ui_messages = {
        GW::UI::UIMessage::kQuestDetailsChanged,
        GW::UI::UIMessage::kQuestAdded,
        GW::UI::UIMessage::kClientActiveQuestChanged,
        GW::UI::UIMessage::kMapLoaded
    };
    for (const auto ui_message : ui_messages) {
        // Post callbacks, non blocking
        GW::UI::RegisterUIMessageCallback(&ui_message_entry, ui_message, OnUIMessage, 0x4000);
    }
    RefreshQuestPath(GW::QuestMgr::GetActiveQuestId());
}

void QuestModule::SignalTerminate()
{
    ToolboxModule::SignalTerminate();
    GW::UI::RemoveUIMessageCallback(&ui_message_entry);
    ClearCalculatedPaths();
}

void QuestModule::Update(float)
{
    const auto pos = GetPlayerPos();
    if (!pos)
        return;
    size_t size = calculated_quest_paths.size();
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

void QuestModule::Terminate()
{
    ToolboxModule::Terminate();
}

QuestObjective::QuestObjective(GW::Constants::QuestID quest_id, const wchar_t* _objective_enc, bool is_completed)
    : quest_id(quest_id),
      is_completed(is_completed)
{
    objective_enc = new GuiUtils::EncString(_objective_enc);
}

QuestObjective::~QuestObjective()
{
    if (objective_enc) delete objective_enc;
}
