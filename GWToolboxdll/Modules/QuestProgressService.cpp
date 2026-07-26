#include <Modules/QuestProgressService.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <unordered_set>

namespace QuestProgress {
namespace {

void ClearCharacter(CharacterProgress& c)
{
    c = CharacterProgress{};
}

StoredCharacter* FindCharacter(AccountProgressStore& store, const std::string& key)
{
    auto it = store.characters.find(key);
    return it == store.characters.end() ? nullptr : &it->second;
}

std::chrono::milliseconds BackoffForAttempt(uint32_t attempts)
{
    const auto max_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(kPersistRetryMaxBackoff).count());
    uint64_t ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(kPersistRetryInitialBackoff).count());
    for (uint32_t i = 0; i < attempts && ms < max_ms; ++i) {
        ms *= 2;
        if (ms > max_ms) {
            ms = max_ms;
            break;
        }
    }
    return std::chrono::milliseconds(static_cast<int64_t>(ms));
}

} // namespace

std::string FormatCanonicalUtc(std::chrono::system_clock::time_point wall_now)
{
    const auto ms_total = std::chrono::duration_cast<std::chrono::milliseconds>(wall_now.time_since_epoch());
    const auto sec = std::chrono::duration_cast<std::chrono::seconds>(ms_total);
    const auto millis = static_cast<int>((ms_total - sec).count());
    const std::time_t t = static_cast<std::time_t>(sec.count());
    std::tm tm{};
#if defined(_WIN32)
    if (gmtime_s(&tm, &t) != 0) {
        return {};
    }
#else
    if (!gmtime_r(&t, &tm)) {
        return {};
    }
#endif
    char buf[32];
    std::snprintf(
        buf, sizeof(buf),
        "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec, millis);
    return buf;
}

bool QuestProgressService::IsPermanentBlockStatus(StoreOpStatus status)
{
    return status == StoreOpStatus::CodecError
        || status == StoreOpStatus::UnsupportedDiskMajor
        || status == StoreOpStatus::ValidationError;
}

bool QuestProgressService::IsRetryableBlockStatus(StoreOpStatus status)
{
    return status == StoreOpStatus::LockTimeout
        || status == StoreOpStatus::LockFailed
        || status == StoreOpStatus::IoError
        || status == StoreOpStatus::MergeConflict;
}

void QuestProgressService::SetStoreDirectory(std::filesystem::path directory)
{
    store_directory_ = std::move(directory);
}

void QuestProgressService::AddDiag(std::string message)
{
    if (!diagnostics_.empty() && diagnostics_.back() == message) {
        return;
    }
    if (diagnostics_.size() >= kMaxDiagnostics) {
        diagnostics_.erase(diagnostics_.begin());
    }
    diagnostics_.push_back(std::move(message));
}

void QuestProgressService::NotePersistStatus(StoreOpStatus status, const char* operation)
{
    if (has_persist_status_ && last_persist_status_ == status) {
        return;
    }
    has_persist_status_ = true;
    last_persist_status_ = status;
    char buf[160];
    std::snprintf(
        buf, sizeof(buf),
        "persist %s status=%u gen_a=%llu gen_c=%llu",
        operation,
        static_cast<unsigned>(status),
        static_cast<unsigned long long>(account_generation_),
        static_cast<unsigned long long>(character_generation_));
    AddDiag(buf);
}

void QuestProgressService::Initialize()
{
    if (initialized_) {
        return;
    }
    initialized_ = true;
    terminate_signaled_ = false;
    accept_input_ = true;
    final_flush_requested_ = false;
    diagnostics_.clear();
    has_persist_status_ = false;
}

void QuestProgressService::SignalTerminate()
{
    accept_input_ = false;
    terminate_signaled_ = true;
    final_flush_requested_ = true;
}

void QuestProgressService::Terminate()
{
    accept_input_ = false;
    terminate_signaled_ = true;
    if (final_flush_requested_ || semantic_dirty_ || heartbeat_pending_) {
        if (!Flush(true) && (semantic_dirty_ || heartbeat_pending_)
            && identity_.kind == IdentityKind::Persistent) {
            RetainActiveAsDetached(last_persist_status_);
            ClearActiveSessionMemory();
        }
    }
    // Best-effort detached flush; retain leftovers rather than discard.
    for (size_t i = 0; i < detached_sessions_.size();) {
        if (TryPersistDetached(i, last_tick_steady_)) {
            // erased inside success path
        }
        else {
            ++i;
        }
    }
    pending_evidence_.clear();
    ClearCharacter(character_);
    account_store_ = {};
    identity_ = {};
    load_status_ = StoreOpStatus::Empty;
    persistence_allowed_ = false;
    semantic_dirty_ = false;
    heartbeat_pending_ = false;
    has_reduced_once_ = false;
    last_reduced_revision_ = 0;
    ResetActivePersistLatch();
    initialized_ = false;
    final_flush_requested_ = false;
}

void QuestProgressService::ResetActivePersistLatch()
{
    persist_latch_ = PersistLatch::Clean;
    persist_retry_attempts_ = 0;
    next_persist_retry_at_ = {};
    has_persist_status_ = false;
    last_persist_status_ = StoreOpStatus::Ok;
}

void QuestProgressService::ClearActiveSessionMemory()
{
    pending_evidence_.clear();
    semantic_dirty_ = false;
    heartbeat_pending_ = false;
    dirty_generation_ = 0;
    ClearCharacter(character_);
    has_reduced_once_ = false;
    last_reduced_revision_ = 0;
    ResetActivePersistLatch();
}

void QuestProgressService::MarkSemanticDirty(std::chrono::steady_clock::time_point steady_now)
{
    ++dirty_generation_;
    semantic_dirty_ = true;
    last_semantic_change_ = steady_now;
    heartbeat_pending_ = false;
    if (persist_latch_ != PersistLatch::BlockedPermanent) {
        persist_latch_ = PersistLatch::DirtyDebouncing;
    }
}

void QuestProgressService::RetainActiveAsDetached(StoreOpStatus last_status)
{
    if (identity_.kind != IdentityKind::Persistent) {
        return;
    }
    SyncCharacterIntoAccountStore();
    DetachedDirtySession detached;
    detached.identity = identity_;
    detached.account_generation = account_generation_;
    detached.character_generation = character_generation_;
    detached.account_store = account_store_;
    detached.character = character_;
    detached.semantic_dirty = semantic_dirty_;
    detached.heartbeat_pending = heartbeat_pending_;
    detached.dirty_generation = dirty_generation_;
    detached.last_status = last_status;
    detached.latch = IsPermanentBlockStatus(last_status)
        ? PersistLatch::BlockedPermanent
        : PersistLatch::BlockedRetryable;
    detached.retry_attempts = 0;
    detached.next_retry_at = last_tick_steady_ + kPersistRetryInitialBackoff;
    detached.pending_evidence = pending_evidence_;
    detached_sessions_.push_back(std::move(detached));
    AddDiag("retained detached dirty session after failed flush");
}

void QuestProgressService::SyncCharacterIntoAccountStore()
{
    if (identity_.kind != IdentityKind::Persistent || identity_.character_key.empty()) {
        return;
    }
    auto* stored = FindCharacter(account_store_, identity_.character_key);
    if (!stored) {
        return;
    }
    stored->display_name = identity_.display_name;
    stored->profession = identity_.profession;
    stored->is_pre_searing = identity_.is_pre_searing;
    stored->quests = character_.quests;
    if (!character_.last_reduced_at.empty()) {
        stored->last_observed_at = character_.last_reduced_at;
        if (stored->first_observed_at.empty()) {
            stored->first_observed_at = character_.last_reduced_at;
        }
    }
}

void QuestProgressService::EnsureCharacterRecord()
{
    if (identity_.kind != IdentityKind::Persistent) {
        return;
    }
    auto* stored = FindCharacter(account_store_, identity_.character_key);
    if (!stored) {
        StoredCharacter created;
        created.character_key = identity_.character_key;
        created.display_name = identity_.display_name;
        created.profession = identity_.profession;
        created.is_pre_searing = identity_.is_pre_searing;
        account_store_.characters.emplace(identity_.character_key, std::move(created));
        stored = FindCharacter(account_store_, identity_.character_key);
    }
    else {
        if (!identity_.display_name.empty()) {
            stored->display_name = identity_.display_name;
        }
        if (!identity_.profession.empty()) {
            stored->profession = identity_.profession;
        }
        if (identity_.is_pre_searing) {
            stored->is_pre_searing = identity_.is_pre_searing;
        }
    }
    character_.character_key = identity_.character_key;
    character_.display_name = stored->display_name;
    character_.quests = stored->quests;
    character_.last_reduced_at = stored->last_observed_at;
}

void QuestProgressService::LoadAccountForIdentity()
{
    persistence_allowed_ = false;
    account_store_ = {};
    account_store_.store_format = kStoreFormatId;
    account_store_.store_version = {kStoreFormatMajor, kStoreFormatMinor};
    account_store_.account_key = identity_.account_key;
    ResetActivePersistLatch();

    if (identity_.kind != IdentityKind::Persistent) {
        load_status_ = StoreOpStatus::Empty;
        AddDiag("ephemeral or unbound identity; persistence disabled");
        return;
    }
    if (store_directory_.empty()) {
        load_status_ = StoreOpStatus::ValidationError;
        persist_latch_ = PersistLatch::BlockedPermanent;
        AddDiag("store directory not set; persistence disabled");
        return;
    }

    const auto loaded = LoadAccountStore(store_directory_, identity_.account_key);
    load_status_ = loaded.status;
    for (const auto& m : loaded.diagnostics.messages) {
        AddDiag(m);
    }

    switch (loaded.status) {
        case StoreOpStatus::Empty:
            account_store_.account_key = identity_.account_key;
            persistence_allowed_ = true;
            AddDiag("empty account store; first save may create primary");
            break;
        case StoreOpStatus::Ok:
        case StoreOpStatus::RecoveredFromBak:
            account_store_ = loaded.store;
            account_store_.account_key = identity_.account_key;
            persistence_allowed_ = true;
            if (loaded.status == StoreOpStatus::RecoveredFromBak) {
                AddDiag("loaded account store from backup");
            }
            break;
        case StoreOpStatus::CodecError:
            persistence_allowed_ = false;
            persist_latch_ = PersistLatch::BlockedPermanent;
            NotePersistStatus(StoreOpStatus::CodecError, "load");
            AddDiag("account store CodecError; automatic save blocked");
            break;
        case StoreOpStatus::UnsupportedDiskMajor:
            persistence_allowed_ = false;
            persist_latch_ = PersistLatch::BlockedPermanent;
            NotePersistStatus(StoreOpStatus::UnsupportedDiskMajor, "load");
            AddDiag("account store UnsupportedDiskMajor; automatic save blocked");
            break;
        default:
            persistence_allowed_ = false;
            if (IsRetryableBlockStatus(loaded.status)) {
                persist_latch_ = PersistLatch::BlockedRetryable;
            }
            else {
                persist_latch_ = PersistLatch::BlockedPermanent;
            }
            NotePersistStatus(loaded.status, "load");
            AddDiag("account store load failed; automatic save blocked");
            break;
    }
}

void QuestProgressService::BindIdentity(const SessionIdentity& next, bool force_session_gap)
{
    if (!accept_input_ && !terminate_signaled_) {
        return;
    }

    if (SamePersistentCharacter(identity_, next) && !force_session_gap) {
        // Map-load / metadata refresh without character switch.
        identity_.display_name = next.display_name;
        identity_.profession = next.profession;
        identity_.is_pre_searing = next.is_pre_searing;
        if (auto* stored = FindCharacter(account_store_, identity_.character_key)) {
            if (!next.display_name.empty()) {
                stored->display_name = next.display_name;
            }
        }
        character_.display_name = identity_.display_name;
        return;
    }

    const bool leaving_bound = identity_.kind != IdentityKind::Unbound;
    if (leaving_bound) {
        // Apply scoped pending evidence to the outgoing session before flush/retain.
        FinalizeOutgoingEvidence();
        // Flush or retain — never discard dirty old-session progress.
        if (identity_.kind == IdentityKind::Persistent
            && (semantic_dirty_ || heartbeat_pending_)) {
            if (!Flush(true)) {
                RetainActiveAsDetached(
                    has_persist_status_ ? last_persist_status_ : StoreOpStatus::IoError);
            }
        }
        ClearActiveSessionMemory();
    }

    const bool account_changed = !SameAccount(identity_, next);
    const auto previous_account = identity_.account_key;
    identity_ = next;
    ++character_generation_;
    if (account_changed || previous_account != identity_.account_key) {
        ++account_generation_;
    }
    session_gap_pending_ = force_session_gap || leaving_bound;
    has_reduced_once_ = false;
    last_reduced_revision_ = 0;

    char gen_buf[96];
    std::snprintf(
        gen_buf, sizeof(gen_buf),
        "identity bind gen_a=%llu gen_c=%llu kind=%u",
        static_cast<unsigned long long>(account_generation_),
        static_cast<unsigned long long>(character_generation_),
        static_cast<unsigned>(identity_.kind));
    AddDiag(gen_buf);

    if (identity_.kind == IdentityKind::Unbound) {
        account_store_ = {};
        load_status_ = StoreOpStatus::Empty;
        persistence_allowed_ = false;
        ResetActivePersistLatch();
        AddDiag("identity unbound");
        return;
    }

    if (identity_.kind == IdentityKind::Ephemeral) {
        account_store_ = {};
        load_status_ = StoreOpStatus::Empty;
        persistence_allowed_ = false;
        character_.character_key.clear();
        character_.display_name = identity_.display_name;
        ResetActivePersistLatch();
        AddDiag("ephemeral identity; no persistent file I/O");
        return;
    }

    if (account_changed || account_store_.account_key != identity_.account_key) {
        LoadAccountForIdentity();
    }
    else {
        // Same account, new character: do not inherit prior permanent block from another char
        // unless the loaded store itself is permanently blocked.
        if (load_status_ == StoreOpStatus::CodecError
            || load_status_ == StoreOpStatus::UnsupportedDiskMajor) {
            persistence_allowed_ = false;
            persist_latch_ = PersistLatch::BlockedPermanent;
        }
        else if (!persistence_allowed_
                 && !IsPermanentBlockStatus(load_status_)) {
            LoadAccountForIdentity();
        }
        else {
            ResetActivePersistLatch();
            if (persistence_allowed_) {
                persist_latch_ = PersistLatch::Clean;
            }
        }
    }
    EnsureCharacterRecord();
}

void QuestProgressService::UnbindIdentity()
{
    if (!accept_input_ && !terminate_signaled_) {
        return;
    }
    if (identity_.kind == IdentityKind::Unbound) {
        return;
    }

    FinalizeOutgoingEvidence();
    if (identity_.kind == IdentityKind::Persistent
        && (semantic_dirty_ || heartbeat_pending_)) {
        if (!Flush(true)) {
            RetainActiveAsDetached(
                has_persist_status_ ? last_persist_status_ : StoreOpStatus::IoError);
        }
    }
    ClearActiveSessionMemory();
    account_store_ = {};
    identity_ = {};
    ++character_generation_;
    ++account_generation_;
    load_status_ = StoreOpStatus::Empty;
    persistence_allowed_ = false;
    session_gap_pending_ = false;
    ResetActivePersistLatch();
    AddDiag("explicit logout unbound");
}

void QuestProgressService::FinalizeOutgoingEvidence()
{
    if (identity_.kind == IdentityKind::Unbound || !has_reduced_once_ || pending_evidence_.empty()) {
        return;
    }

    const auto wall_now = std::chrono::system_clock::now();
    const auto steady_now = std::chrono::steady_clock::now();
    last_tick_steady_ = steady_now;
    const auto observed_at = FormatCanonicalUtc(wall_now);
    if (!IsCanonicalUtcTimestamp(observed_at)) {
        AddDiag("refusing outgoing evidence finalize: non-canonical timestamp");
        return;
    }

    const auto evidence = CollectActiveEvidence(steady_now);
    if (evidence.empty()) {
        return;
    }

    std::unordered_set<uint32_t> actionable;
    for (const auto& e : evidence) {
        if (e.kind == EvidenceKind::Abandon || e.kind == EvidenceKind::Reward) {
            actionable.insert(e.game_quest_id);
        }
    }
    if (actionable.empty()) {
        return;
    }

    // Keep non-evidence quests present so only actionable IDs take the missing/evidence path.
    ReducerInput input;
    input.previous = character_;
    input.observed_at_utc = observed_at;
    input.session_gap = false;
    input.treat_as_stale_if_older = true;
    input.evidence = evidence;
    for (const auto& [quest_id, quest] : character_.quests) {
        if (actionable.count(quest_id)) {
            continue;
        }
        if (quest.state == ProgressState::AbandonedObserved
            || quest.state == ProgressState::CompletedObserved) {
            continue;
        }
        LiveQuestObservation obs;
        obs.game_quest_id = quest_id;
        obs.in_log_completed = quest.state == ProgressState::ReadyForReward;
        obs.objectives_missing = quest.objectives.empty();
        obs.objectives = quest.objectives;
        input.observed_quests.push_back(std::move(obs));
    }

    const auto out = Reduce(input);
    character_ = out.next;
    character_.character_key =
        identity_.kind == IdentityKind::Persistent ? identity_.character_key : character_.character_key;
    character_.display_name = identity_.display_name;
    for (const auto& m : out.diagnostics.messages) {
        AddDiag(m);
    }
    if (out.semantic_changed) {
        MarkSemanticDirty(steady_now);
    }
    else if (out.touch_last_observed) {
        heartbeat_pending_ = true;
    }
    SyncCharacterIntoAccountStore();

    pending_evidence_.erase(
        std::remove_if(
            pending_evidence_.begin(), pending_evidence_.end(),
            [&](const EvidenceStamp& e) {
                return e.character_generation == character_generation_
                    && actionable.count(e.game_quest_id) != 0;
            }),
        pending_evidence_.end());
    AddDiag("finalized outgoing scoped evidence");
}

void QuestProgressService::RouteOrDropEvidence(EvidenceStamp stamp)
{
    if (stamp.game_quest_id == 0 || IsSyntheticQuestId(stamp.game_quest_id)
        || stamp.kind == EvidenceKind::None) {
        return;
    }

    // Unscoped stamps attach to the current bound generation when bound.
    if (stamp.account_generation == 0 && stamp.character_generation == 0) {
        if (identity_.kind == IdentityKind::Unbound) {
            AddDiag("dropped unscoped evidence while unbound");
            return;
        }
        stamp.account_generation = account_generation_;
        stamp.character_generation = character_generation_;
        stamp.account_key = identity_.account_key;
        stamp.character_key = identity_.character_key;
    }

    if (stamp.character_generation == character_generation_
        && stamp.account_generation == account_generation_
        && identity_.kind != IdentityKind::Unbound) {
        pending_evidence_.push_back(std::move(stamp));
        return;
    }

    for (auto& detached : detached_sessions_) {
        if (detached.character_generation == stamp.character_generation
            && detached.account_generation == stamp.account_generation) {
            detached.pending_evidence.push_back(std::move(stamp));
            AddDiag("routed evidence to detached session");
            return;
        }
    }

    AddDiag("dropped stale/unscoped evidence");
}

void QuestProgressService::IngestEvidence(std::vector<EvidenceStamp> stamps)
{
    if (!accept_input_) {
        return;
    }
    for (auto& s : stamps) {
        RouteOrDropEvidence(std::move(s));
    }
}

void QuestProgressService::ExpireEvidence(std::chrono::steady_clock::time_point steady_now)
{
    pending_evidence_.erase(
        std::remove_if(
            pending_evidence_.begin(), pending_evidence_.end(),
            [&](const EvidenceStamp& e) {
                return steady_now - e.steady_at > kEvidencePairingWindow;
            }),
        pending_evidence_.end());

    for (auto& detached : detached_sessions_) {
        detached.pending_evidence.erase(
            std::remove_if(
                detached.pending_evidence.begin(), detached.pending_evidence.end(),
                [&](const EvidenceStamp& e) {
                    return steady_now - e.steady_at > kEvidencePairingWindow;
                }),
            detached.pending_evidence.end());
    }
}

std::vector<QuestEvidence> QuestProgressService::CollectActiveEvidence(
    std::chrono::steady_clock::time_point steady_now)
{
    ExpireEvidence(steady_now);
    std::vector<QuestEvidence> out;
    out.reserve(pending_evidence_.size());
    for (const auto& e : pending_evidence_) {
        if (e.character_generation != character_generation_
            || e.account_generation != account_generation_) {
            continue;
        }
        QuestEvidence qe;
        qe.game_quest_id = e.game_quest_id;
        qe.kind = e.kind;
        out.push_back(qe);
    }
    return out;
}

void QuestProgressService::ReduceFromSnapshot(
    const QuestSnapshot& snap,
    std::chrono::system_clock::time_point wall_now,
    std::chrono::steady_clock::time_point steady_now)
{
    if (identity_.kind == IdentityKind::Unbound) {
        return;
    }
    if (snap.loading || !snap.world_ready) {
        return;
    }
    if (has_reduced_once_ && snap.revision == last_reduced_revision_) {
        return;
    }

    const auto observed_at = FormatCanonicalUtc(wall_now);
    if (!IsCanonicalUtcTimestamp(observed_at)) {
        AddDiag("refusing reduce: non-canonical wall clock timestamp");
        return;
    }

    last_tick_steady_ = steady_now;

    ReducerInput input;
    input.previous = character_;
    input.observed_at_utc = observed_at;
    input.session_gap = session_gap_pending_;
    input.treat_as_stale_if_older = true;
    input.evidence = CollectActiveEvidence(steady_now);

    for (const auto& q : snap.quests) {
        if (IsSyntheticQuestId(q.game_quest_id) || q.game_quest_id == 0) {
            continue;
        }
        LiveQuestObservation obs;
        obs.game_quest_id = q.game_quest_id;
        obs.in_log_completed = q.in_log_completed;
        obs.objectives_missing = q.objectives_missing;
        obs.objectives = q.objectives;
        input.observed_quests.push_back(std::move(obs));
    }

    const auto out = Reduce(input);
    character_ = out.next;
    character_.character_key =
        identity_.kind == IdentityKind::Persistent ? identity_.character_key : character_.character_key;
    character_.display_name = identity_.display_name;
    last_reduced_revision_ = snap.revision;
    has_reduced_once_ = true;
    session_gap_pending_ = false;

    for (const auto& m : out.diagnostics.messages) {
        AddDiag(m);
    }

    if (out.semantic_changed) {
        MarkSemanticDirty(steady_now);
    }
    else if (out.touch_last_observed) {
        heartbeat_pending_ = true;
        if (persist_latch_ == PersistLatch::Clean) {
            persist_latch_ = PersistLatch::DirtyDebouncing;
        }
    }

    SyncCharacterIntoAccountStore();
}

void QuestProgressService::IngestSnapshot(
    const QuestSnapshot& snap,
    std::chrono::system_clock::time_point wall_now,
    std::chrono::steady_clock::time_point steady_now)
{
    if (!accept_input_) {
        return;
    }
    ReduceFromSnapshot(snap, wall_now, steady_now);
}

void QuestProgressService::ScheduleRetryBackoff(std::chrono::steady_clock::time_point steady_now)
{
    const auto delay = BackoffForAttempt(persist_retry_attempts_);
    next_persist_retry_at_ = steady_now + delay;
    ++persist_retry_attempts_;
    persist_latch_ = PersistLatch::BlockedRetryable;
}

bool QuestProgressService::ShouldAttemptPersist(
    std::chrono::steady_clock::time_point steady_now, bool session_boundary) const
{
    if (persist_latch_ == PersistLatch::BlockedPermanent) {
        return false;
    }
    if (!persistence_allowed_ && identity_.kind == IdentityKind::Persistent) {
        if (IsPermanentBlockStatus(load_status_)) {
            return false;
        }
    }
    if (persist_latch_ == PersistLatch::BlockedRetryable && !session_boundary) {
        if (next_persist_retry_at_.time_since_epoch().count() == 0) {
            return false;
        }
        if (steady_now < next_persist_retry_at_) {
            return false;
        }
    }
    return true;
}

void QuestProgressService::Tick(
    std::chrono::steady_clock::time_point steady_now,
    std::chrono::system_clock::time_point /*wall_now*/)
{
    if (!initialized_) {
        return;
    }
    last_tick_steady_ = steady_now;
    ExpireEvidence(steady_now);
    TickDetachedRetries(steady_now);

    if (final_flush_requested_) {
        Flush(true);
        final_flush_requested_ = false;
        return;
    }

    if (!accept_input_) {
        return;
    }

    if (semantic_dirty_) {
        if (persist_latch_ == PersistLatch::DirtyDebouncing
            && (steady_now - last_semantic_change_) >= kSemanticSaveDebounce) {
            persist_latch_ = PersistLatch::ReadyToSave;
        }
        if ((persist_latch_ == PersistLatch::ReadyToSave
             || persist_latch_ == PersistLatch::BlockedRetryable
             || persist_latch_ == PersistLatch::DirtyDebouncing)
            && (steady_now - last_semantic_change_) >= kSemanticSaveDebounce) {
            if (ShouldAttemptPersist(steady_now, false)) {
                TryPersist(false, steady_now);
            }
        }
    }
    else if (heartbeat_pending_ && persistence_allowed_
             && identity_.kind == IdentityKind::Persistent
             && persist_latch_ != PersistLatch::BlockedPermanent) {
        if (last_heartbeat_save_.time_since_epoch().count() == 0
            || (steady_now - last_heartbeat_save_) >= kHeartbeatInterval) {
            if (ShouldAttemptPersist(steady_now, false)) {
                TryPersist(false, steady_now);
            }
        }
    }
}

bool QuestProgressService::TryPersistDetached(
    size_t index, std::chrono::steady_clock::time_point steady_now)
{
    if (index >= detached_sessions_.size()) {
        return false;
    }
    auto& detached = detached_sessions_[index];
    if (!detached.semantic_dirty && !detached.heartbeat_pending) {
        detached_sessions_.erase(detached_sessions_.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }
    if (detached.latch == PersistLatch::BlockedPermanent) {
        return false;
    }
    if (detached.latch == PersistLatch::BlockedRetryable && steady_now < detached.next_retry_at) {
        return false;
    }
    if (detached.identity.kind != IdentityKind::Persistent || store_directory_.empty()) {
        return false;
    }

    ++persist_attempt_count_;
    detached.account_store.account_key = detached.identity.account_key;
    detached.account_store.store_format = kStoreFormatId;
    detached.account_store.store_version = {kStoreFormatMajor, kStoreFormatMinor};

    // Ensure character blob is synced from retained character progress.
    if (auto* stored = FindCharacter(detached.account_store, detached.identity.character_key)) {
        stored->quests = detached.character.quests;
        if (!detached.character.last_reduced_at.empty()) {
            stored->last_observed_at = detached.character.last_reduced_at;
        }
    }

    const uint64_t gen_at_start = detached.dirty_generation;
    const auto saved = SaveMergedAccountStore(
        store_directory_, detached.identity.account_key, detached.account_store, lock_timeout_ms_);
    for (const auto& m : saved.diagnostics.messages) {
        AddDiag(m);
    }

    if (saved.status != StoreOpStatus::Ok || !saved.merged.has_value()) {
        ++blocked_save_count_;
        detached.last_status = saved.status;
        if (IsPermanentBlockStatus(saved.status)) {
            detached.latch = PersistLatch::BlockedPermanent;
            NotePersistStatus(saved.status, "detached");
        }
        else {
            detached.latch = PersistLatch::BlockedRetryable;
            detached.next_retry_at = steady_now + BackoffForAttempt(detached.retry_attempts);
            ++detached.retry_attempts;
            NotePersistStatus(saved.status, "detached");
        }
        return false;
    }

    if (detached.dirty_generation == gen_at_start) {
        detached.semantic_dirty = false;
        detached.heartbeat_pending = false;
    }
    detached.account_store = *saved.merged;
    ++successful_save_count_;
    AddDiag("detached session flush ok");
    detached_sessions_.erase(detached_sessions_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

void QuestProgressService::TickDetachedRetries(std::chrono::steady_clock::time_point steady_now)
{
    for (size_t i = 0; i < detached_sessions_.size();) {
        auto& d = detached_sessions_[i];
        if (d.latch == PersistLatch::BlockedPermanent) {
            ++i;
            continue;
        }
        if (!d.semantic_dirty && !d.heartbeat_pending) {
            detached_sessions_.erase(detached_sessions_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        if (d.latch == PersistLatch::BlockedRetryable && steady_now < d.next_retry_at) {
            ++i;
            continue;
        }
        if (!TryPersistDetached(i, steady_now)) {
            ++i;
        }
        // On success, index i is the next element after erase.
    }
}

bool QuestProgressService::TryPersist(bool session_boundary)
{
    return TryPersist(session_boundary, last_tick_steady_.time_since_epoch().count() == 0
        ? std::chrono::steady_clock::now()
        : last_tick_steady_);
}

bool QuestProgressService::TryPersist(
    bool session_boundary, std::chrono::steady_clock::time_point steady_now)
{
    last_flush_ok_ = false;
    last_tick_steady_ = steady_now;

    if (identity_.kind != IdentityKind::Persistent) {
        semantic_dirty_ = false;
        heartbeat_pending_ = false;
        dirty_generation_ = 0;
        persist_latch_ = PersistLatch::Clean;
        return false;
    }
    if (!ShouldAttemptPersist(steady_now, session_boundary)) {
        if (persist_latch_ == PersistLatch::BlockedPermanent
            || IsPermanentBlockStatus(load_status_)) {
            ++blocked_save_count_;
            NotePersistStatus(
                IsPermanentBlockStatus(load_status_) ? load_status_ : last_persist_status_,
                "blocked");
        }
        return false;
    }

    ++persist_attempt_count_;

    if (!persistence_allowed_) {
        ++blocked_save_count_;
        if (IsPermanentBlockStatus(load_status_)) {
            persist_latch_ = PersistLatch::BlockedPermanent;
            NotePersistStatus(load_status_, "blocked");
        }
        else {
            ScheduleRetryBackoff(steady_now);
            NotePersistStatus(load_status_, "blocked");
        }
        return false;
    }
    if (store_directory_.empty()) {
        ++blocked_save_count_;
        persist_latch_ = PersistLatch::BlockedPermanent;
        NotePersistStatus(StoreOpStatus::ValidationError, "blocked");
        return false;
    }

    persist_latch_ = PersistLatch::Saving;
    const uint64_t gen_at_start = dirty_generation_;
    if (persist_test_hook_) {
        persist_test_hook_();
    }

    SyncCharacterIntoAccountStore();
    account_store_.account_key = identity_.account_key;
    account_store_.store_format = kStoreFormatId;
    account_store_.store_version = {kStoreFormatMajor, kStoreFormatMinor};

    const auto saved = SaveMergedAccountStore(
        store_directory_, identity_.account_key, account_store_, lock_timeout_ms_);
    for (const auto& m : saved.diagnostics.messages) {
        AddDiag(m);
    }

    if (saved.status != StoreOpStatus::Ok || !saved.merged.has_value()) {
        ++blocked_save_count_;
        NotePersistStatus(saved.status, "save");
        if (IsPermanentBlockStatus(saved.status)) {
            persist_latch_ = PersistLatch::BlockedPermanent;
            persistence_allowed_ = false;
        }
        else {
            ScheduleRetryBackoff(steady_now);
        }
        return false;
    }

    account_store_ = *saved.merged;
    EnsureCharacterRecord();
    // Clear dirty only for the generation captured at save start.
    if (dirty_generation_ == gen_at_start) {
        semantic_dirty_ = false;
        heartbeat_pending_ = false;
        persist_latch_ = PersistLatch::Clean;
        persist_retry_attempts_ = 0;
        next_persist_retry_at_ = {};
    }
    else {
        semantic_dirty_ = true;
        persist_latch_ = PersistLatch::DirtyDebouncing;
        last_semantic_change_ = steady_now;
    }
    last_heartbeat_save_ = steady_now;
    ++successful_save_count_;
    last_flush_ok_ = true;
    has_persist_status_ = true;
    last_persist_status_ = StoreOpStatus::Ok;
    if (session_boundary) {
        AddDiag("session-boundary flush ok");
    }
    return true;
}

bool QuestProgressService::Flush(bool session_boundary)
{
    if (!semantic_dirty_ && !heartbeat_pending_ && !session_boundary) {
        return true;
    }
    if (!semantic_dirty_ && !heartbeat_pending_ && session_boundary
        && identity_.kind != IdentityKind::Persistent) {
        return true;
    }
    if (!semantic_dirty_ && !heartbeat_pending_ && session_boundary && !persistence_allowed_) {
        return false;
    }
    return TryPersist(session_boundary);
}

} // namespace QuestProgress
