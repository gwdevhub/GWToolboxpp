#include "stdafx.h"

#include <Modules/QuestObservationService.h>
#include <Modules/AudioSettings.h>
#include <Utils/ToolboxUtils.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/QuestMgr.h>

namespace {
    void BlockQuestInfoSound()
    {
        AudioSettings::BlockSoundForMs(L"\xe14d\x0101", 1000);
        AudioSettings::BlockSoundForMs(L"\xe14c\x0101", 1000);
    }
}

void QuestObservationService::Initialize()
{
    if (callbacks_registered_) {
        MarkAllDirty();
        return;
    }
    terminated_ = false;
    RegisterCallbacks();
    MarkAllDirty();
    published_loading_invalid_ = false;
}

void QuestObservationService::RegisterCallbacks()
{
    if (callbacks_registered_) return;

    constexpr GW::UI::UIMessage messages[] = {
        GW::UI::UIMessage::kQuestAdded,
        GW::UI::UIMessage::kQuestDetailsChanged,
        GW::UI::UIMessage::kQuestRemoved,
        GW::UI::UIMessage::kClientActiveQuestChanged,
        GW::UI::UIMessage::kServerActiveQuestChanged,
        GW::UI::UIMessage::kObjectiveAdd,
        GW::UI::UIMessage::kObjectiveComplete,
        GW::UI::UIMessage::kObjectiveUpdated,
        GW::UI::UIMessage::kStartMapLoad,
        GW::UI::UIMessage::kMapLoaded,
    };

    for (const auto message_id : messages) {
        GW::UI::RegisterUIMessageCallback(
            &ui_hook_entry_, message_id,
            [this](GW::HookStatus* status, GW::UI::UIMessage msg, void* wparam, void* lparam) {
                OnUIMessage(status, msg, wparam, lparam);
            },
            0x4000);
    }
    callbacks_registered_ = true;
}

void QuestObservationService::UnregisterCallbacks()
{
    if (!callbacks_registered_) return;
    GW::UI::RemoveUIMessageCallback(&ui_hook_entry_);
    callbacks_registered_ = false;
}

void QuestObservationService::SignalTerminate()
{
    UnregisterCallbacks();
    terminated_ = true;
}

void QuestObservationService::Terminate()
{
    UnregisterCallbacks();
    terminated_ = true;
    request_state_.clear();
    clear_inflight_ids_.clear();
    quest_log_dirty_ = false;
    active_quest_dirty_ = false;
    mission_objectives_dirty_ = false;
    published_loading_invalid_ = false;

    auto empty = std::make_shared<LiveQuestView>();
    empty->revision = next_revision_++;
    empty->loading = true;
    empty->world_ready = false;
    empty->active_quest_id = GW::Constants::QuestID::None;
    Publish(std::shared_ptr<const LiveQuestView>(std::move(empty)));
}

void QuestObservationService::MarkAllDirty()
{
    quest_log_dirty_ = true;
    active_quest_dirty_ = true;
    mission_objectives_dirty_ = true;
}

void QuestObservationService::Publish(std::shared_ptr<const LiveQuestView> view)
{
    std::scoped_lock lock(snapshot_mutex_);
    published_ = std::move(view);
}

std::shared_ptr<const LiveQuestView> QuestObservationService::AcquireSnapshot() const
{
    std::scoped_lock lock(snapshot_mutex_);
    return published_;
}

void QuestObservationService::PublishLoadingInvalid()
{
    auto view = std::make_shared<LiveQuestView>();
    view->revision = next_revision_++;
    view->loading = true;
    view->world_ready = false;
    view->active_quest_id = GW::Constants::QuestID::None;
    view->mission_mode = false;
    Publish(std::shared_ptr<const LiveQuestView>(std::move(view)));
    published_loading_invalid_ = true;
}

bool QuestObservationService::IsWorldReady() const
{
    if (GW::UI::IsLoadingScreenShown()) return false;
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return false;
    if (!GW::Map::GetIsMapLoaded()) return false;
    if (!GW::GetWorldContext()) return false;
    return true;
}

std::wstring QuestObservationService::CopyEnc(const wchar_t* enc)
{
    if (!enc) return {};
    return std::wstring(enc);
}

void QuestObservationService::ParseQuestObjectivesOwned(const wchar_t* objectives, std::vector<OwnedObjective>& out)
{
    out.clear();
    if (!objectives) return;

    const wchar_t* current = objectives;
    while (current) {
        const wchar_t* next = wcschr(current, 0x2);
        const size_t len = next ? static_cast<size_t>(next - current) : wcslen(current);
        std::wstring enc_str(current, len);
        auto content_start = enc_str.find(0x10a);
        if (content_start == std::wstring::npos) break;
        content_start++;
        if (content_start >= enc_str.size()) break;
        enc_str = enc_str.substr(content_start, enc_str.size() - content_start - 1);

        OwnedObjective obj;
        obj.completed = (*current == 0x2af5);
        obj.encoded = std::move(enc_str);
        out.push_back(std::move(obj));

        current = next ? next + 1 : nullptr;
    }
}

void QuestObservationService::SnapshotQuestLog(LiveQuestView& view) const
{
    view.quests.clear();
    const auto* log = GW::QuestMgr::GetQuestLog();
    if (!log) return;

    for (const auto& quest : *log) {
        if (quest.quest_id == custom_marker_quest_id) continue;

        OwnedQuestEntry entry;
        entry.quest_id = quest.quest_id;
        entry.log_state = quest.log_state;
        entry.in_log_completed = (quest.log_state & 0x2) != 0;
        entry.name_encoded = CopyEnc(quest.name);
        if (!quest.objectives) {
            entry.objectives_missing = true;
        }
        else {
            ParseQuestObjectivesOwned(quest.objectives, entry.objectives);
        }
        view.quests.push_back(std::move(entry));
    }
}

void QuestObservationService::SnapshotActiveQuest(LiveQuestView& view) const
{
    const auto qid = GW::QuestMgr::GetActiveQuestId();
    view.active_quest_id = qid;
    view.mission_mode = static_cast<int32_t>(qid) == -1;
    if (!view.mission_mode && qid == custom_marker_quest_id) {
        // Treat custom marker as no real selection for tracker highlighting
        view.active_quest_id = GW::Constants::QuestID::None;
    }
}

void QuestObservationService::SnapshotMissionObjectives(LiveQuestView& view) const
{
    view.mission_objectives.clear();
    const auto* world = GW::GetWorldContext();
    if (!world) return;

    for (const auto& objective : world->mission_objectives) {
        if (!objective.enc_str) continue;
        OwnedMissionObjective owned;
        owned.objective_id = objective.objective_id;
        owned.enc = CopyEnc(objective.enc_str);
        owned.type = objective.type;
        owned.bullet = (objective.type & OBJECTIVE_FLAG_BULLET) != 0;
        owned.completed = (objective.type & OBJECTIVE_FLAG_COMPLETED) != 0;
        if (owned.bullet) {
            view.mission_objectives.push_back(std::move(owned));
        }
    }
}

LiveQuestView QuestObservationService::SnapshotAllChannels(uint64_t revision) const
{
    LiveQuestView view;
    view.revision = revision;
    view.loading = false;
    view.world_ready = true;
    SnapshotQuestLog(view);
    SnapshotActiveQuest(view);
    SnapshotMissionObjectives(view);
    return view;
}

void QuestObservationService::RequestMissingQuestInfo(const LiveQuestView& view)
{
    if (terminated_ || !IsWorldReady()) return;

    const auto now = std::chrono::steady_clock::now();
    for (const auto& quest : view.quests) {
        if (!quest.objectives_missing) continue;
        if (quest.quest_id == custom_marker_quest_id) continue;
        if (quest.quest_id == GW::Constants::QuestID::None) continue;

        auto& state = request_state_[quest.quest_id];
        if (state.requested_since_details) continue;
        if (state.last_request.time_since_epoch().count() != 0 && now - state.last_request < request_cooldown) {
            continue;
        }

        BlockQuestInfoSound();
        GW::QuestMgr::RequestQuestInfoId(quest.quest_id, false);
        state.last_request = now;
        state.requested_since_details = true;
    }
}

void QuestObservationService::OnUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*)
{
    if (terminated_) return;

    switch (message_id) {
        case GW::UI::UIMessage::kQuestAdded:
        case GW::UI::UIMessage::kQuestRemoved:
            quest_log_dirty_ = true;
            active_quest_dirty_ = true;
            break;
        case GW::UI::UIMessage::kQuestDetailsChanged: {
            quest_log_dirty_ = true;
            if (wparam) {
                const auto quest_id = *static_cast<GW::Constants::QuestID*>(wparam);
                clear_inflight_ids_.insert(quest_id);
            }
        } break;
        case GW::UI::UIMessage::kClientActiveQuestChanged:
        case GW::UI::UIMessage::kServerActiveQuestChanged:
            active_quest_dirty_ = true;
            break;
        case GW::UI::UIMessage::kObjectiveAdd:
        case GW::UI::UIMessage::kObjectiveComplete:
        case GW::UI::UIMessage::kObjectiveUpdated:
            mission_objectives_dirty_ = true;
            break;
        case GW::UI::UIMessage::kStartMapLoad:
            MarkAllDirty();
            PublishLoadingInvalid();
            break;
        case GW::UI::UIMessage::kMapLoaded:
            MarkAllDirty();
            published_loading_invalid_ = false;
            break;
        default:
            break;
    }
}

void QuestObservationService::Update(float)
{
    if (terminated_) return;

    for (const auto quest_id : clear_inflight_ids_) {
        if (auto it = request_state_.find(quest_id); it != request_state_.end()) {
            it->second.requested_since_details = false;
        }
    }
    clear_inflight_ids_.clear();

    const bool any_dirty = quest_log_dirty_ || active_quest_dirty_ || mission_objectives_dirty_;
    if (!any_dirty) return;

    if (!IsWorldReady()) {
        if (!published_loading_invalid_) {
            PublishLoadingInvalid();
        }
        // Keep dirty flags set until a successful ready snapshot
        return;
    }

    // Prefer full refresh when all channels dirty or no prior ready view
    const auto previous = AcquireSnapshot();
    const bool need_full = !previous || !previous->world_ready
        || (quest_log_dirty_ && active_quest_dirty_ && mission_objectives_dirty_);

    LiveQuestView local;
    if (need_full) {
        local = SnapshotAllChannels(next_revision_++);
    }
    else {
        local = *previous;
        local.revision = next_revision_++;
        local.loading = false;
        local.world_ready = true;
        if (quest_log_dirty_) SnapshotQuestLog(local);
        if (active_quest_dirty_) SnapshotActiveQuest(local);
        if (mission_objectives_dirty_) SnapshotMissionObjectives(local);
    }

    auto published = std::make_shared<const LiveQuestView>(std::move(local));
    Publish(published);
    published_loading_invalid_ = false;

    quest_log_dirty_ = false;
    active_quest_dirty_ = false;
    mission_objectives_dirty_ = false;

    RequestMissingQuestInfo(*published);
}
