#include "stdafx.h"

#include <Windows/QuestTrackerWindow.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/QuestMgr.h>

#include <Utils/FontLoader.h>

namespace {
    constexpr ImU32 TEXT_COLOR_COMPLETED = 0xffbbbbbb;
    constexpr ImU32 TEXT_COLOR_ACTIVE = 0xff00ff00;
    constexpr ImU32 TEXT_COLOR_READY = 0xff66ccff;
    constexpr auto custom_marker_quest_id = static_cast<GW::Constants::QuestID>(0x0000fdd);

    void EnqueueSetActiveQuest(GW::Constants::QuestID quest_id)
    {
        GW::GameThread::Enqueue([quest_id] {
            if (quest_id == GW::Constants::QuestID::None) return;
            if (quest_id == custom_marker_quest_id) return;
            if (!GW::Map::GetIsMapLoaded()) return;
            if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;
            if (!GW::QuestMgr::GetQuest(quest_id)) return;
            if (GW::QuestMgr::GetActiveQuestId() == quest_id) return;
            GW::QuestMgr::SetActiveQuestId(quest_id);
        });
    }
}

void QuestTrackerWindow::Initialize()
{
    terminating_ = false;
    ToolboxWindow::Initialize();
    observation_.Initialize();
}

void QuestTrackerWindow::Update(float delta)
{
    observation_.Update(delta);
}

void QuestTrackerWindow::SignalTerminate()
{
    terminating_ = true;
    observation_.SignalTerminate();
    ToolboxWindow::SignalTerminate();
}

void QuestTrackerWindow::Terminate()
{
    terminating_ = true;
    ClearDecodeCache();
    observation_.Terminate();
    ToolboxWindow::Terminate();
}

void QuestTrackerWindow::ClearDecodeCache()
{
    name_decoders_.clear();
    quest_objective_decoders_.clear();
    mission_objective_decoders_.clear();
    cached_revision_ = 0;
}

void QuestTrackerWindow::SyncDecodeCache(const LiveQuestView& view)
{
    if (cached_revision_ == view.revision) return;
    cached_revision_ = view.revision;

    std::unordered_map<GW::Constants::QuestID, std::unique_ptr<GuiUtils::EncString>> next_names;
    std::unordered_map<ObjectiveKey, std::unique_ptr<GuiUtils::EncString>, ObjectiveKeyHash> next_objectives;
    std::unordered_map<uint32_t, std::unique_ptr<GuiUtils::EncString>> next_mission;

    for (const auto& quest : view.quests) {
        if (auto it = name_decoders_.find(quest.quest_id); it != name_decoders_.end()) {
            next_names.emplace(quest.quest_id, std::move(it->second));
        }
        for (size_t i = 0; i < quest.objectives.size(); ++i) {
            ObjectiveKey key{quest.quest_id, i};
            if (auto it = quest_objective_decoders_.find(key); it != quest_objective_decoders_.end()) {
                next_objectives.emplace(key, std::move(it->second));
            }
        }
    }
    for (const auto& objective : view.mission_objectives) {
        if (auto it = mission_objective_decoders_.find(objective.objective_id); it != mission_objective_decoders_.end()) {
            next_mission.emplace(objective.objective_id, std::move(it->second));
        }
    }

    name_decoders_ = std::move(next_names);
    quest_objective_decoders_ = std::move(next_objectives);
    mission_objective_decoders_ = std::move(next_mission);
}

GuiUtils::EncString& QuestTrackerWindow::NameDecoder(GW::Constants::QuestID quest_id, const std::wstring& encoded)
{
    auto& ptr = name_decoders_[quest_id];
    if (!ptr) {
        ptr = std::make_unique<GuiUtils::EncString>(encoded.empty() ? nullptr : encoded.c_str());
    }
    else if (ptr->encoded() != encoded) {
        ptr->reset(encoded.empty() ? nullptr : encoded.c_str());
    }
    return *ptr;
}

GuiUtils::EncString& QuestTrackerWindow::QuestObjectiveDecoder(
    GW::Constants::QuestID quest_id, size_t index, const std::wstring& encoded)
{
    ObjectiveKey key{quest_id, index};
    auto& ptr = quest_objective_decoders_[key];
    if (!ptr) {
        ptr = std::make_unique<GuiUtils::EncString>(encoded.empty() ? nullptr : encoded.c_str());
    }
    else if (ptr->encoded() != encoded) {
        ptr->reset(encoded.empty() ? nullptr : encoded.c_str());
    }
    return *ptr;
}

GuiUtils::EncString& QuestTrackerWindow::MissionObjectiveDecoder(uint32_t objective_id, const std::wstring& encoded)
{
    auto& ptr = mission_objective_decoders_[objective_id];
    if (!ptr) {
        ptr = std::make_unique<GuiUtils::EncString>(encoded.empty() ? nullptr : encoded.c_str());
    }
    else if (ptr->encoded() != encoded) {
        ptr->reset(encoded.empty() ? nullptr : encoded.c_str());
    }
    return *ptr;
}

void QuestTrackerWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) return;

    ImGui::SetNextWindowSize(ImVec2(320.f, 400.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        ImGui::End();
        return;
    }

    const auto snap = observation_.AcquireSnapshot();
    if (!snap || snap->loading || !snap->world_ready) {
        ImGui::TextUnformatted("Loading...");
        ImGui::End();
        return;
    }

    SyncDecodeCache(*snap);

    ImGui::TextUnformatted("Quest log");
    ImGui::Separator();

    const bool allow_select = !terminating_ && !snap->mission_mode;

    if (snap->quests.empty()) {
        ImGui::TextDisabled("No quests in log");
    }
    else {
        for (const auto& quest : snap->quests) {
            const bool is_active = !snap->mission_mode && quest.quest_id == snap->active_quest_id;
            auto& name_decoder = NameDecoder(quest.quest_id, quest.name_encoded);
            const char* name = name_decoder.string().c_str();
            if (!name || !*name) {
                name = name_decoder.IsDecoding() ? "..." : "(unnamed quest)";
            }

            ImGui::PushID(static_cast<int>(static_cast<uint32_t>(quest.quest_id)));
            const ImVec2 row_pos = ImGui::GetCursorScreenPos();
            const float row_width = ImGui::GetContentRegionAvail().x;
            const float row_height = ImGui::GetTextLineHeightWithSpacing();

            if (allow_select) {
                if (ImGui::InvisibleButton("##quest_row", ImVec2(row_width, row_height))) {
                    if (!is_active) {
                        const auto quest_id = quest.quest_id;
                        EnqueueSetActiveQuest(quest_id);
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Click to set active quest");
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }
                ImGui::SetCursorScreenPos(row_pos);
            }

            if (is_active) {
                ImGui::PushStyleColor(ImGuiCol_Text, TEXT_COLOR_ACTIVE);
            }
            ImGui::TextUnformatted(name);
            if (is_active) {
                ImGui::PopStyleColor();
            }

            if (quest.in_log_completed) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, TEXT_COLOR_READY);
                ImGui::TextUnformatted("[ready]");
                ImGui::PopStyleColor();
            }

            if (allow_select) {
                ImGui::SetCursorScreenPos(ImVec2(row_pos.x, row_pos.y + row_height));
            }
            ImGui::PopID();
        }
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Active quest details");
    ImGui::Separator();

    if (snap->mission_mode) {
        ImGui::TextDisabled("Mission mode active — see mission objectives below");
    }
    else if (snap->active_quest_id == GW::Constants::QuestID::None) {
        ImGui::TextDisabled("No active quest selected");
    }
    else {
        const OwnedQuestEntry* active = nullptr;
        for (const auto& quest : snap->quests) {
            if (quest.quest_id == snap->active_quest_id) {
                active = &quest;
                break;
            }
        }
        if (!active) {
            ImGui::TextDisabled("Active quest not present in log");
        }
        else if (active->objectives_missing) {
            ImGui::TextDisabled("Objectives pending...");
        }
        else if (active->objectives.empty()) {
            ImGui::TextDisabled("No objectives");
        }
        else {
            for (size_t i = 0; i < active->objectives.size(); ++i) {
                const auto& objective = active->objectives[i];
                auto& decoder = QuestObjectiveDecoder(active->quest_id, i, objective.encoded);
                const char* text = decoder.string().c_str();
                if (!text || !*text) {
                    text = decoder.IsDecoding() ? "..." : "(objective)";
                }
                if (objective.completed) {
                    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_COLOR_COMPLETED);
                }
                ImGui::Bullet();
                ImGui::TextUnformatted(text);
                if (objective.completed) {
                    ImGui::PopStyleColor();
                }
            }
        }
    }

    if (snap->mission_mode) {
        ImGui::Spacing();
        ImGui::TextUnformatted("Mission objectives");
        ImGui::Separator();

        if (snap->mission_objectives.empty()) {
            ImGui::TextDisabled("No mission objectives");
        }
        else {
            for (const auto& objective : snap->mission_objectives) {
                auto& decoder = MissionObjectiveDecoder(objective.objective_id, objective.enc);
                const char* text = decoder.string().c_str();
                if (!text || !*text) {
                    text = decoder.IsDecoding() ? "..." : "(objective)";
                }
                if (objective.completed) {
                    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_COLOR_COMPLETED);
                }
                ImGui::Bullet();
                ImGui::TextUnformatted(text);
                if (objective.completed) {
                    ImGui::PopStyleColor();
                }
            }
        }
    }

    ImGui::End();
}
