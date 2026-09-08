#include "stdafx.h"

#include <Modules/QuestObservationService.h>
#include <Modules/AudioSettings.h>
#include <Modules/QuestChatEvidence.h>
#include <Modules/QuestSessionIdentity.h>
#include <Utils/ToolboxUtils.h>
#include <Utils/TextUtils.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/CharContext.h>
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
    loading_transition_pending_ = false;
    request_cycle_reset_pending_ = false;
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
        GW::UI::UIMessage::kSendAbandonQuest,
        GW::UI::UIMessage::kSendDialog,
        GW::UI::UIMessage::kLogout,
        GW::UI::UIMessage::kLogChatMessage,
        GW::UI::UIMessage::kWriteToChatLog,
        GW::UI::UIMessage::kDungeonComplete,
        GW::UI::UIMessage::kMissionComplete,
        GW::UI::UIMessage::kVanquishComplete,
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
    abandon_probes_.Clear();
    abandon_probe_diagnostics_.clear();
}

void QuestObservationService::Terminate()
{
    UnregisterCallbacks();
    terminated_ = true;
    request_state_.clear();
    quest_log_dirty_ = false;
    active_quest_dirty_ = false;
    mission_objectives_dirty_ = false;
    loading_transition_pending_ = false;
    request_cycle_reset_pending_ = false;
    published_loading_invalid_ = false;
    {
        std::scoped_lock lock(evidence_mutex_);
        pending_evidence_.clear();
        pending_journey_hints_.clear();
        logout_pending_ = false;
    }
    abandon_probes_.Clear();
    abandon_probe_diagnostics_.clear();

    auto empty = std::make_shared<LiveQuestView>();
    empty->revision = next_revision_++;
    empty->loading = true;
    empty->world_ready = false;
    empty->active_quest_id = GW::Constants::QuestID::None;
    Publish(std::shared_ptr<const LiveQuestView>(std::move(empty)));
}

void QuestObservationService::ScheduleAbandonProbe(uint32_t quest_id)
{
    if (terminated_ || quest_id == 0 || quest_id == static_cast<uint32_t>(custom_marker_quest_id)) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    abandon_probes_.OnAbandon(quest_id, now);

    // Do not wait for kQuestRemoved — request an immediate re-observation, then bounded retries.
    quest_log_dirty_ = true;

    const auto snap = AcquireSnapshot();
    if (snap && snap->world_ready && !snap->loading
        && static_cast<uint32_t>(snap->active_quest_id) == quest_id) {
        active_quest_dirty_ = true;
    }
    else {
        // May still be selected; cheap to refresh active channel alongside the log.
        active_quest_dirty_ = true;
    }
}

void QuestObservationService::ResolveAbandonProbes(
    const LiveQuestView& view, std::chrono::steady_clock::time_point now)
{
    // Map load / world-not-ready must not count as disappearance.
    if (!view.world_ready || view.loading) {
        return;
    }
    std::unordered_set<uint32_t> present;
    present.reserve(view.quests.size());
    for (const auto& q : view.quests) {
        present.insert(static_cast<uint32_t>(q.quest_id));
    }
    // Bounded timeout diagnostics retained for runtime verification (cap 8).
    std::vector<std::string> diagnostics;
    abandon_probes_.OnWorldReadyQuestLog(now, present, &diagnostics);
    for (auto& line : diagnostics) {
        if (abandon_probe_diagnostics_.size() >= 8) {
            break;
        }
        abandon_probe_diagnostics_.push_back(std::move(line));
    }
}

void QuestObservationService::PushEvidence(uint32_t quest_id, QuestProgress::EvidenceKind kind)
{
    if (terminated_ || quest_id == 0 || quest_id == static_cast<uint32_t>(custom_marker_quest_id)) {
        return;
    }
    QuestEvidenceStamp stamp;
    stamp.game_quest_id = quest_id;
    stamp.kind = kind;
    stamp.steady_at = std::chrono::steady_clock::now();
    std::scoped_lock lock(evidence_mutex_);
    pending_evidence_.push_back(stamp);
}

void QuestObservationService::PushJourneyMilestoneHint(std::string_view kind, uint32_t map_id)
{
    if (terminated_ || map_id == 0) {
        return;
    }
    if (kind != "dungeon_complete" && kind != "mission_complete" && kind != "vanquish_complete") {
        return;
    }
    JourneyMilestoneHint hint;
    hint.kind = std::string(kind);
    hint.map_id = map_id;
    hint.wall_at = std::chrono::system_clock::now();
    std::scoped_lock lock(evidence_mutex_);
    pending_journey_hints_.push_back(std::move(hint));
}

uint32_t QuestObservationService::ResolveQuestIdFromLiveLog(const wchar_t* name_argument) const
{
    if (!name_argument || !*name_argument) {
        return 0;
    }
    const auto* log = GW::QuestMgr::GetQuestLog();
    if (!log) {
        return 0;
    }
    std::vector<std::pair<uint32_t, std::wstring>> names;
    names.reserve(log->size());
    for (const auto& quest : *log) {
        if (quest.quest_id == custom_marker_quest_id || quest.quest_id == GW::Constants::QuestID::None) {
            continue;
        }
        if (!quest.name) {
            continue;
        }
        names.emplace_back(static_cast<uint32_t>(quest.quest_id), std::wstring(quest.name));
    }
    return QuestChatEvidence::ResolveQuestIdByEncodedName(name_argument, names);
}

void QuestObservationService::OnChatEvidenceMessage(const wchar_t* message)
{
    if (terminated_ || !message || !*message) {
        return;
    }
    const auto chat_kind = QuestChatEvidence::Classify(message);
    if (chat_kind == QuestChatEvidence::MessageKind::None) {
        return;
    }

    const auto* name_argument = QuestChatEvidence::ExtractQuestNameArgument(message);
    const auto quest_id = ResolveQuestIdFromLiveLog(name_argument);
    if (quest_id == 0) {
        return;
    }

    const auto evidence_kind = chat_kind == QuestChatEvidence::MessageKind::RewardAccepted
        ? QuestProgress::EvidenceKind::ChatReward
        : QuestProgress::EvidenceKind::ChatUpdated;

    const auto now = std::chrono::steady_clock::now();
    if (recent_chat_evidence_.quest_id == quest_id
        && recent_chat_evidence_.kind == evidence_kind
        && now - recent_chat_evidence_.at < std::chrono::milliseconds(250)) {
        return;
    }
    recent_chat_evidence_ = {quest_id, evidence_kind, now};

    PushEvidence(quest_id, evidence_kind);
    if (evidence_kind == QuestProgress::EvidenceKind::ChatUpdated) {
        quest_log_dirty_ = true;
    }
}

void QuestObservationService::DrainPendingEvidence(std::vector<QuestEvidenceStamp>& out)
{
    std::scoped_lock lock(evidence_mutex_);
    out.insert(out.end(), pending_evidence_.begin(), pending_evidence_.end());
    pending_evidence_.clear();
}

void QuestObservationService::DrainPendingJourneyHints(std::vector<JourneyMilestoneHint>& out)
{
    std::scoped_lock lock(evidence_mutex_);
    out.insert(out.end(), pending_journey_hints_.begin(), pending_journey_hints_.end());
    pending_journey_hints_.clear();
}

bool QuestObservationService::ConsumeLogoutSignal()
{
    std::scoped_lock lock(evidence_mutex_);
    const bool pending = logout_pending_;
    logout_pending_ = false;
    return pending;
}

void QuestObservationService::MarkAllDirty()
{
    quest_log_dirty_ = true;
    active_quest_dirty_ = true;
    mission_objectives_dirty_ = true;
}

void QuestObservationService::RequestFullRefresh()
{
    if (terminated_) {
        return;
    }
    MarkAllDirty();
}

void QuestObservationService::StampOwnedIdentity(LiveQuestView& view) const
{
    view.identity_captured = false;
    view.account_key.clear();
    view.character_key.clear();

    // Copy owned UUID strings only — never retain CharContext pointers past this call.
    const auto account_guid = GW::AccountMgr::GetAccountUuid();
    const auto account_str = TextUtils::GuidToString(&account_guid);

    QuestProgress::UuidWords character_words{};
    if (const auto* ctx = GW::GetCharContext()) {
        character_words = QuestProgress::UuidWords{
            ctx->player_uuid[0], ctx->player_uuid[1], ctx->player_uuid[2], ctx->player_uuid[3]};
    }

    const auto identity = QuestProgress::MakeSessionIdentity(
        account_str,
        QuestProgress::FormatUuidWords(character_words),
        {},
        {},
        std::nullopt);
    if (identity.kind == QuestProgress::IdentityKind::Unbound) {
        return;
    }
    view.identity_captured = true;
    view.account_key = identity.account_key;
    view.character_key = identity.character_key;
}


void QuestObservationService::ResetRequestAttemptCycle()
{
    request_state_.clear();
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

void QuestObservationService::SyncPendingRequestsFromSnapshot(const LiveQuestView& view)
{
    std::unordered_set<GW::Constants::QuestID> still_missing;
    for (const auto& quest : view.quests) {
        if (!quest.objectives_missing) continue;
        if (quest.quest_id == custom_marker_quest_id) continue;
        if (quest.quest_id == GW::Constants::QuestID::None) continue;
        still_missing.insert(quest.quest_id);
        request_state_.try_emplace(quest.quest_id);
    }

    for (auto it = request_state_.begin(); it != request_state_.end();) {
        if (still_missing.contains(it->first)) {
            ++it;
        }
        else {
            it = request_state_.erase(it);
        }
    }
}

void QuestObservationService::ProcessPendingRequests()
{
    if (terminated_ || !IsWorldReady() || request_state_.empty()) return;

    const auto now = std::chrono::steady_clock::now();
    for (auto& [quest_id, state] : request_state_) {
        if (state.attempts >= max_request_attempts) continue;
        if (state.last_request.time_since_epoch().count() != 0 && now - state.last_request < request_cooldown) {
            continue;
        }

        BlockQuestInfoSound();
        GW::QuestMgr::RequestQuestInfoId(quest_id, false);
        state.last_request = now;
        ++state.attempts;
    }
}

void QuestObservationService::OnUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*)
{
    if (terminated_) return;

    switch (message_id) {
        case GW::UI::UIMessage::kQuestAdded: {
            quest_log_dirty_ = true;
            active_quest_dirty_ = true;
            if (wparam) {
                const auto quest_id = *static_cast<uint32_t*>(wparam);
                PushEvidence(quest_id, QuestProgress::EvidenceKind::Accepted);
            }
            break;
        }
        case GW::UI::UIMessage::kQuestRemoved:
            quest_log_dirty_ = true;
            active_quest_dirty_ = true;
            break;
        case GW::UI::UIMessage::kQuestDetailsChanged:
            // Snapshot decides whether the request remains pending
            quest_log_dirty_ = true;
            break;
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
            loading_transition_pending_ = true;
            request_cycle_reset_pending_ = true;
            break;
        case GW::UI::UIMessage::kMapLoaded:
            MarkAllDirty();
            published_loading_invalid_ = false;
            request_cycle_reset_pending_ = true;
            break;
        case GW::UI::UIMessage::kSendAbandonQuest: {
            const auto quest_id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(wparam));
            PushEvidence(quest_id, QuestProgress::EvidenceKind::Abandon);
            ScheduleAbandonProbe(quest_id);
            break;
        }
        case GW::UI::UIMessage::kSendDialog: {
            // Dialog bit layout mirrors DialogModule (avoid private API coupling).
            const auto dialog_id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(wparam));
            if ((dialog_id & 0x800000) == 0) {
                break;
            }
            const auto quest_id = (dialog_id ^ 0x800000) >> 8;
            const auto dialog_type = dialog_id & 0xf0000f;
            if (dialog_type == 0x800007) {
                PushEvidence(quest_id, QuestProgress::EvidenceKind::Reward);
            }
            else if (dialog_type == 0x800006) {
                PushEvidence(quest_id, QuestProgress::EvidenceKind::EnquireReward);
            }
            break;
        }
        case GW::UI::UIMessage::kLogout: {
            // Character-select / session end — not kStartMapLoad.
            std::scoped_lock lock(evidence_mutex_);
            logout_pending_ = true;
            break;
        }
        case GW::UI::UIMessage::kLogChatMessage: {
            const auto packet = static_cast<GW::UI::UIPacket::kLogChatMessage*>(wparam);
            if (packet && packet->message) {
                OnChatEvidenceMessage(packet->message);
            }
            break;
        }
        case GW::UI::UIMessage::kWriteToChatLog: {
            const auto packet = static_cast<GW::UI::UIPacket::kWriteToChatLog*>(wparam);
            if (packet && packet->message) {
                OnChatEvidenceMessage(packet->message);
            }
            break;
        }
        case GW::UI::UIMessage::kDungeonComplete: {
            if (GW::Map::GetIsMapLoaded()) {
                PushJourneyMilestoneHint(
                    "dungeon_complete",
                    static_cast<uint32_t>(GW::Map::GetMapID()));
            }
            break;
        }
        case GW::UI::UIMessage::kMissionComplete: {
            if (GW::Map::GetIsMapLoaded()) {
                PushJourneyMilestoneHint(
                    "mission_complete",
                    static_cast<uint32_t>(GW::Map::GetMapID()));
            }
            break;
        }
        case GW::UI::UIMessage::kVanquishComplete: {
            if (GW::Map::GetIsMapLoaded()) {
                PushJourneyMilestoneHint(
                    "vanquish_complete",
                    static_cast<uint32_t>(GW::Map::GetMapID()));
            }
            break;
        }
        default:
            break;
    }
}

void QuestObservationService::Update(float)
{
    if (terminated_) return;

    if (request_cycle_reset_pending_) {
        ResetRequestAttemptCycle();
        request_cycle_reset_pending_ = false;
    }

    if (loading_transition_pending_) {
        PublishLoadingInvalid();
        loading_transition_pending_ = false;
        // Dirty flags remain set until a successful world-ready snapshot
    }

    const auto now = std::chrono::steady_clock::now();
    const bool world_ready = IsWorldReady();
    // Bounded abandon re-observation (not every frame): mark dirty when a probe offset is due.
    if (abandon_probes_.Tick(now, world_ready)) {
        quest_log_dirty_ = true;
    }

    const bool any_dirty = quest_log_dirty_ || active_quest_dirty_ || mission_objectives_dirty_;

    if (any_dirty) {
        if (!world_ready) {
            if (!published_loading_invalid_) {
                PublishLoadingInvalid();
            }
            // Keep dirty flags set; do not process pending requests while not ready
            // Abandon probes remain suspended (Tick already no-op'd).
            return;
        }

        const auto previous = AcquireSnapshot();
        const bool need_full = !previous || !previous->world_ready
            || (quest_log_dirty_ && active_quest_dirty_ && mission_objectives_dirty_);
        const bool snapshotted_quest_log = need_full || quest_log_dirty_;

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
        StampOwnedIdentity(local);

        auto published = std::make_shared<const LiveQuestView>(std::move(local));
        Publish(published);
        published_loading_invalid_ = false;

        quest_log_dirty_ = false;
        active_quest_dirty_ = false;
        mission_objectives_dirty_ = false;

        if (snapshotted_quest_log) {
            SyncPendingRequestsFromSnapshot(*published);
            ResolveAbandonProbes(*published, now);
        }
    }

    // Process due pending requests even when no snapshot channel is dirty
    ProcessPendingRequests();
}
