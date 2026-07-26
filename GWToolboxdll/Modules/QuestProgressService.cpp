#include <Modules/QuestProgressService.h>

#include <algorithm>
#include <cstdio>
#include <ctime>

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

void QuestProgressService::SetStoreDirectory(std::filesystem::path directory)
{
    store_directory_ = std::move(directory);
}

void QuestProgressService::AddDiag(std::string message)
{
    diagnostics_.push_back(std::move(message));
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
        Flush(true);
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
    initialized_ = false;
    final_flush_requested_ = false;
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

    if (identity_.kind != IdentityKind::Persistent) {
        load_status_ = StoreOpStatus::Empty;
        AddDiag("ephemeral or unbound identity; persistence disabled");
        return;
    }
    if (store_directory_.empty()) {
        load_status_ = StoreOpStatus::ValidationError;
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
            AddDiag("account store CodecError; automatic save blocked");
            break;
        case StoreOpStatus::UnsupportedDiskMajor:
            persistence_allowed_ = false;
            AddDiag("account store UnsupportedDiskMajor; automatic save blocked");
            break;
        default:
            persistence_allowed_ = false;
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

    // Leaving a previous durable character: flush before rebinding.
    if (identity_.kind == IdentityKind::Persistent && persistence_allowed_
        && (semantic_dirty_ || heartbeat_pending_)) {
        Flush(true);
    }

    const bool account_changed = !SameAccount(identity_, next);
    identity_ = next;
    session_gap_pending_ = force_session_gap || has_reduced_once_;
    has_reduced_once_ = false;
    last_reduced_revision_ = 0;
    pending_evidence_.clear();
    semantic_dirty_ = false;
    heartbeat_pending_ = false;
    ClearCharacter(character_);

    if (identity_.kind == IdentityKind::Unbound) {
        account_store_ = {};
        load_status_ = StoreOpStatus::Empty;
        persistence_allowed_ = false;
        AddDiag("identity unbound");
        return;
    }

    if (identity_.kind == IdentityKind::Ephemeral) {
        account_store_ = {};
        load_status_ = StoreOpStatus::Empty;
        persistence_allowed_ = false;
        character_.character_key.clear();
        character_.display_name = identity_.display_name;
        AddDiag("ephemeral identity; no persistent file I/O");
        return;
    }

    if (account_changed || account_store_.account_key != identity_.account_key) {
        LoadAccountForIdentity();
    }
    else if (!persistence_allowed_ && load_status_ != StoreOpStatus::CodecError
             && load_status_ != StoreOpStatus::UnsupportedDiskMajor) {
        LoadAccountForIdentity();
    }
    EnsureCharacterRecord();
}

void QuestProgressService::IngestEvidence(std::vector<EvidenceStamp> stamps)
{
    if (!accept_input_) {
        return;
    }
    for (auto& s : stamps) {
        if (s.game_quest_id == 0 || IsSyntheticQuestId(s.game_quest_id) || s.kind == EvidenceKind::None) {
            continue;
        }
        pending_evidence_.push_back(std::move(s));
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
}

std::vector<QuestEvidence> QuestProgressService::CollectActiveEvidence(
    std::chrono::steady_clock::time_point steady_now)
{
    ExpireEvidence(steady_now);
    std::vector<QuestEvidence> out;
    out.reserve(pending_evidence_.size());
    for (const auto& e : pending_evidence_) {
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
    character_.character_key = identity_.kind == IdentityKind::Persistent ? identity_.character_key : character_.character_key;
    character_.display_name = identity_.display_name;
    last_reduced_revision_ = snap.revision;
    has_reduced_once_ = true;
    session_gap_pending_ = false;

    for (const auto& m : out.diagnostics.messages) {
        AddDiag(m);
    }

    if (out.semantic_changed) {
        semantic_dirty_ = true;
        last_semantic_change_ = steady_now;
        heartbeat_pending_ = false;
    }
    else if (out.touch_last_observed) {
        heartbeat_pending_ = true;
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

void QuestProgressService::Tick(
    std::chrono::steady_clock::time_point steady_now,
    std::chrono::system_clock::time_point /*wall_now*/)
{
    if (!initialized_) {
        return;
    }
    last_tick_steady_ = steady_now;
    ExpireEvidence(steady_now);

    if (final_flush_requested_) {
        Flush(true);
        final_flush_requested_ = false;
        return;
    }

    if (!accept_input_) {
        return;
    }

    if (semantic_dirty_ && (steady_now - last_semantic_change_) >= kSemanticSaveDebounce) {
        TryPersist(false);
    }
    else if (heartbeat_pending_ && persistence_allowed_
             && identity_.kind == IdentityKind::Persistent) {
        if (last_heartbeat_save_.time_since_epoch().count() == 0
            || (steady_now - last_heartbeat_save_) >= kHeartbeatInterval) {
            TryPersist(false);
        }
    }
}

bool QuestProgressService::TryPersist(bool session_boundary)
{
    last_flush_ok_ = false;
    if (identity_.kind != IdentityKind::Persistent) {
        // Ephemeral must never create files.
        semantic_dirty_ = false;
        heartbeat_pending_ = false;
        return false;
    }
    if (!persistence_allowed_) {
        ++blocked_save_count_;
        AddDiag("persist blocked by load/store status");
        return false;
    }
    if (store_directory_.empty()) {
        ++blocked_save_count_;
        AddDiag("persist blocked: empty store directory");
        return false;
    }

    SyncCharacterIntoAccountStore();
    account_store_.account_key = identity_.account_key;
    account_store_.store_format = kStoreFormatId;
    account_store_.store_version = {kStoreFormatMajor, kStoreFormatMinor};

    const auto saved = SaveMergedAccountStore(store_directory_, identity_.account_key, account_store_);
    for (const auto& m : saved.diagnostics.messages) {
        AddDiag(m);
    }

    if (saved.status != StoreOpStatus::Ok || !saved.merged.has_value()) {
        ++blocked_save_count_;
        AddDiag("SaveMergedAccountStore did not publish");
        // Retain dirty memory; do not clear semantic_dirty on failure.
        return false;
    }

    account_store_ = *saved.merged;
    EnsureCharacterRecord();
    semantic_dirty_ = false;
    heartbeat_pending_ = false;
    last_heartbeat_save_ = last_tick_steady_;
    ++successful_save_count_;
    last_flush_ok_ = true;
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
    // Boundary flush still no-ops file create for ephemeral.
    if (!semantic_dirty_ && !heartbeat_pending_ && session_boundary
        && identity_.kind != IdentityKind::Persistent) {
        return true;
    }
    if (!semantic_dirty_ && !heartbeat_pending_ && session_boundary && !persistence_allowed_) {
        return false;
    }
    if (!semantic_dirty_ && heartbeat_pending_ && !session_boundary) {
        // non-boundary heartbeat path goes through TryPersist throttle in Tick
    }
    return TryPersist(session_boundary);
}

} // namespace QuestProgress
