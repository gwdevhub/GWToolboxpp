#include <Modules/QuestProgressService.h>
#include <Modules/QuestMissionSnapshot.h>
#include <Modules/QuestCharacterJourney.h>

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

const StoredCharacter* FindCharacter(const AccountProgressStore& store, const std::string& key)
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

bool LatchBlocksAutoRetry(PersistLatch latch)
{
    return latch == PersistLatch::BlockedPermanent || latch == PersistLatch::NeedsIntervention;
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

bool QuestProgressService::IsInterventionStatus(StoreOpStatus status)
{
    return status == StoreOpStatus::MergeConflict;
}

bool QuestProgressService::IsRetryableBlockStatus(StoreOpStatus status)
{
    return status == StoreOpStatus::LockTimeout
        || status == StoreOpStatus::LockFailed
        || status == StoreOpStatus::IoError;
}

DetachedSessionKey QuestProgressService::MakeDetachedKey(const SessionIdentity& identity)
{
    DetachedSessionKey key;
    key.account_key = identity.account_key;
    key.character_key = identity.character_key;
    return key;
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

void QuestProgressService::NotePersistStatus(
    StoreOpStatus status,
    const char* operation,
    uint64_t scope_account_gen,
    uint64_t scope_character_gen,
    std::string_view scope_character_key)
{
    if (has_persist_status_ && last_persist_status_ == status) {
        return;
    }
    has_persist_status_ = true;
    last_persist_status_ = status;
    if (IsPermanentBlockStatus(status) || IsInterventionStatus(status) || IsRetryableBlockStatus(status)) {
        ++blocked_save_count_;
    }
    char buf[256];
    std::snprintf(
        buf, sizeof(buf),
        "persist %s status=%u gen_a=%llu gen_c=%llu char=%.*s",
        operation,
        static_cast<unsigned>(status),
        static_cast<unsigned long long>(scope_account_gen),
        static_cast<unsigned long long>(scope_character_gen),
        static_cast<int>(std::min<size_t>(scope_character_key.size(), 96)),
        scope_character_key.data());
    AddDiag(buf);
}

void QuestProgressService::Initialize()
{
    if (initialized_) {
        return;
    }
    // Re-enable in the same process: keep detached_sessions_ (dirty gens + latch intact).
    initialized_ = true;
    terminate_signaled_ = false;
    accept_input_ = true;
    final_flush_requested_ = false;
    diagnostics_.clear();
    has_persist_status_ = false;
    last_persist_status_ = StoreOpStatus::Ok;
    AddDiag("quest progress service initialized");
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
            RetainActiveAsDetached(has_persist_status_ ? last_persist_status_ : StoreOpStatus::IoError);
            ClearActiveSessionMemory();
        }
    }

    // One orderly attempt per eligible retryable detached entry; never clear failures.
    std::vector<DetachedSessionKey> keys;
    keys.reserve(detached_sessions_.size());
    for (const auto& [key, detached] : detached_sessions_) {
        if (!LatchBlocksAutoRetry(detached.latch) && (detached.semantic_dirty || detached.heartbeat_pending)) {
            keys.push_back(key);
        }
    }
    for (const auto& key : keys) {
        TryPersistDetached(key, last_tick_steady_);
    }
    if (!detached_sessions_.empty()) {
        AddDiag("terminate retained unsaved detached sessions (process-memory only)");
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

void QuestProgressService::ApplyIdentityMetadataBackfill()
{
    // Never replace a valid non-empty name/profession with an empty transient value.
    if (identity_.kind == IdentityKind::Persistent && !identity_.character_key.empty()) {
        if (auto* stored = FindCharacter(account_store_, identity_.character_key)) {
            if (!identity_.display_name.empty()) {
                stored->display_name = identity_.display_name;
            }
            if (!identity_.profession.empty()) {
                stored->profession = identity_.profession;
            }
            if (identity_.is_pre_searing.has_value()) {
                stored->is_pre_searing = identity_.is_pre_searing;
            }
            if (!stored->display_name.empty() && character_.display_name.empty()) {
                character_.display_name = stored->display_name;
            }
            else if (!identity_.display_name.empty()) {
                character_.display_name = identity_.display_name;
            }
        }
        else if (!identity_.display_name.empty()) {
            character_.display_name = identity_.display_name;
        }
    }
    else if (!identity_.display_name.empty()) {
        character_.display_name = identity_.display_name;
    }
}

void QuestProgressService::MarkSemanticDirty(std::chrono::steady_clock::time_point steady_now)
{
    ++dirty_generation_;
    semantic_dirty_ = true;
    last_semantic_change_ = steady_now;
    heartbeat_pending_ = false;
    if (!LatchBlocksAutoRetry(persist_latch_)) {
        persist_latch_ = PersistLatch::DirtyDebouncing;
    }
}

StoredCharacter QuestProgressService::BuildOutgoingStoredCharacter() const
{
    StoredCharacter stored;
    stored.character_key = identity_.character_key;
    stored.display_name = identity_.display_name;
    stored.profession = identity_.profession;
    stored.is_pre_searing = identity_.is_pre_searing;
    stored.quests = character_.quests;
    stored.last_observed_at = character_.last_reduced_at;
    stored.first_observed_at = character_.last_reduced_at;
    if (const auto* existing = FindCharacter(account_store_, identity_.character_key)) {
        if (stored.display_name.empty()) {
            stored.display_name = existing->display_name;
        }
        if (stored.profession.empty()) {
            stored.profession = existing->profession;
        }
        if (!stored.is_pre_searing.has_value()) {
            stored.is_pre_searing = existing->is_pre_searing;
        }
        if (!existing->first_observed_at.empty()) {
            stored.first_observed_at = existing->first_observed_at;
        }
        if (!existing->last_observed_at.empty() && stored.last_observed_at.empty()) {
            stored.last_observed_at = existing->last_observed_at;
        }
        stored.missions = existing->missions;
        stored.titles = existing->titles;
        stored.last_known_level = existing->last_known_level;
        stored.journey_events = existing->journey_events;
    }
    return stored;
}

AccountProgressStore QuestProgressService::BuildMinimalAccountStore(const StoredCharacter& character) const
{
    AccountProgressStore store;
    store.store_format = kStoreFormatId;
    store.store_version = {kStoreFormatMajor, kStoreFormatMinor};
    store.account_key = identity_.account_key;
    store.characters.emplace(character.character_key, character);
    return store;
}

void QuestProgressService::RemoveOutgoingCharacterFromActiveStore()
{
    if (identity_.character_key.empty()) {
        return;
    }
    account_store_.characters.erase(identity_.character_key);
}

void QuestProgressService::RetainActiveAsDetached(StoreOpStatus last_status)
{
    if (identity_.kind != IdentityKind::Persistent || identity_.character_key.empty()) {
        return;
    }
    SyncCharacterIntoAccountStore();
    const auto key = MakeDetachedKey(identity_);
    auto outgoing = BuildOutgoingStoredCharacter();
    auto minimal = BuildMinimalAccountStore(outgoing);

    const auto apply_latch = [&](DetachedDirtySession& detached) {
        if (IsInterventionStatus(last_status) || !detached.conflict_variants.empty()) {
            detached.latch = PersistLatch::NeedsIntervention;
            detached.last_status = StoreOpStatus::MergeConflict;
        }
        else if (IsPermanentBlockStatus(last_status)) {
            detached.latch = PersistLatch::BlockedPermanent;
            detached.last_status = last_status;
        }
        else if (IsRetryableBlockStatus(last_status)) {
            if (!LatchBlocksAutoRetry(detached.latch)) {
                detached.latch = PersistLatch::BlockedRetryable;
                detached.next_retry_at = last_tick_steady_ + kPersistRetryInitialBackoff;
            }
            detached.last_status = last_status;
        }
        else {
            if (!LatchBlocksAutoRetry(detached.latch)) {
                detached.latch = PersistLatch::BlockedRetryable;
                detached.next_retry_at = last_tick_steady_ + kPersistRetryInitialBackoff;
            }
            detached.last_status = last_status;
        }
    };

    auto it = detached_sessions_.find(key);
    if (it == detached_sessions_.end()) {
        DetachedDirtySession detached;
        detached.key = key;
        detached.identity = identity_;
        detached.account_generation = account_generation_;
        detached.character_generation = character_generation_;
        detached.account_store = std::move(minimal);
        detached.character = character_;
        detached.character.character_key = identity_.character_key;
        detached.semantic_dirty = semantic_dirty_;
        detached.heartbeat_pending = heartbeat_pending_;
        detached.dirty_generation = dirty_generation_;
        detached.pending_evidence = pending_evidence_;
        apply_latch(detached);
        detached_sessions_.emplace(key, std::move(detached));
        AddDiag("retained detached dirty session");
    }
    else {
        auto& detached = it->second;
        const auto* existing_char = FindCharacter(detached.account_store, key.character_key);
        StoredCharacter existing_stored = existing_char ? *existing_char : StoredCharacter{};
        if (existing_stored.character_key.empty()) {
            existing_stored.character_key = key.character_key;
        }
        auto coalesced = CoalesceStoredCharacters(existing_stored, outgoing);
        for (const auto& m : coalesced.diagnostics.messages) {
            AddDiag(m);
        }
        for (auto& [sem_key, variants] : coalesced.conflict_variants) {
            auto& dest = detached.conflict_variants[sem_key];
            for (auto& variant : variants) {
                bool dup = false;
                for (const auto& existing_variant : dest) {
                    if (existing_variant.semantic_event_key == variant.semantic_event_key
                        && existing_variant.state == variant.state
                        && existing_variant.event_type == variant.event_type
                        && existing_variant.evidence_kind == variant.evidence_kind
                        && existing_variant.observed_at == variant.observed_at) {
                        dup = true;
                        break;
                    }
                }
                if (!dup) {
                    dest.push_back(std::move(variant));
                }
            }
        }
        detached.account_store = BuildMinimalAccountStore(coalesced.character);
        detached.account_store.account_key = key.account_key;
        detached.character.character_key = key.character_key;
        detached.character.display_name = coalesced.character.display_name;
        detached.character.quests = coalesced.character.quests;
        detached.character.last_reduced_at = coalesced.character.last_observed_at;
        detached.identity = identity_;
        detached.account_generation = account_generation_;
        detached.character_generation = character_generation_;
        detached.semantic_dirty = true;
        detached.dirty_generation = (std::max)(detached.dirty_generation, dirty_generation_);
        if (heartbeat_pending_) {
            detached.heartbeat_pending = true;
        }
        detached.pending_evidence.insert(
            detached.pending_evidence.end(),
            pending_evidence_.begin(), pending_evidence_.end());
        if (coalesced.status == StoreOpStatus::MergeConflict || !detached.conflict_variants.empty()) {
            detached.latch = PersistLatch::NeedsIntervention;
            detached.last_status = StoreOpStatus::MergeConflict;
            NotePersistStatus(
                StoreOpStatus::MergeConflict, "coalesce",
                detached.account_generation, detached.character_generation, detached.key.character_key);
        }
        else {
            apply_latch(detached);
        }
        AddDiag("coalesced detached dirty session");
    }

    // Active store must not keep a stale copy of the outgoing character.
    RemoveOutgoingCharacterFromActiveStore();
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
    if (!identity_.display_name.empty()) {
        stored->display_name = identity_.display_name;
    }
    if (!identity_.profession.empty()) {
        stored->profession = identity_.profession;
    }
    if (identity_.is_pre_searing.has_value()) {
        stored->is_pre_searing = identity_.is_pre_searing;
    }
    stored->quests = character_.quests;
    if (!character_.last_reduced_at.empty()) {
        stored->last_observed_at = character_.last_reduced_at;
        if (stored->first_observed_at.empty()) {
            stored->first_observed_at = character_.last_reduced_at;
        }
    }
}

std::optional<StoredCharacter> QuestProgressService::BuildExportCharacterSnapshot() const
{
    if (identity_.kind != IdentityKind::Persistent || identity_.character_key.empty()) {
        return std::nullopt;
    }
    if (!IsValidPersistentCharacterKey(identity_.character_key)) {
        return std::nullopt;
    }

    StoredCharacter out;
    if (const auto* stored = FindCharacter(account_store_, identity_.character_key)) {
        out = *stored;
    }
    out.character_key = identity_.character_key;
    out.quests = character_.quests;
    if (!character_.last_reduced_at.empty()) {
        out.last_observed_at = character_.last_reduced_at;
        if (out.first_observed_at.empty()) {
            out.first_observed_at = character_.last_reduced_at;
        }
    }
    if (!identity_.display_name.empty()) {
        out.display_name = identity_.display_name;
    }
    else if (out.display_name.empty() && !character_.display_name.empty()) {
        out.display_name = character_.display_name;
    }
    if (!identity_.profession.empty()) {
        out.profession = identity_.profession;
    }
    if (identity_.is_pre_searing.has_value()) {
        out.is_pre_searing = identity_.is_pre_searing;
    }
    return out;
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
        NotePersistStatus(
            StoreOpStatus::ValidationError, "load",
            account_generation_, character_generation_, identity_.character_key);
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
            NotePersistStatus(
                StoreOpStatus::CodecError, "load",
                account_generation_, character_generation_, identity_.character_key);
            AddDiag("account store CodecError; automatic save blocked");
            break;
        case StoreOpStatus::UnsupportedDiskMajor:
            persistence_allowed_ = false;
            persist_latch_ = PersistLatch::BlockedPermanent;
            NotePersistStatus(
                StoreOpStatus::UnsupportedDiskMajor, "load",
                account_generation_, character_generation_, identity_.character_key);
            AddDiag("account store UnsupportedDiskMajor; automatic save blocked");
            break;
        case StoreOpStatus::MergeConflict:
            persistence_allowed_ = false;
            persist_latch_ = PersistLatch::NeedsIntervention;
            NotePersistStatus(
                StoreOpStatus::MergeConflict, "load",
                account_generation_, character_generation_, identity_.character_key);
            AddDiag("account store MergeConflict; NeedsIntervention");
            break;
        default:
            persistence_allowed_ = false;
            if (IsRetryableBlockStatus(loaded.status)) {
                persist_latch_ = PersistLatch::BlockedRetryable;
            }
            else {
                persist_latch_ = PersistLatch::BlockedPermanent;
            }
            NotePersistStatus(
                loaded.status, "load",
                account_generation_, character_generation_, identity_.character_key);
            AddDiag("account store load failed; automatic save blocked");
            break;
    }
}

void QuestProgressService::BindIdentity(
    const SessionIdentity& next,
    bool force_session_gap,
    uint64_t reject_revision_at_or_below,
    bool require_fresh_identity_snapshot)
{
    if (!accept_input_ && !terminate_signaled_) {
        return;
    }

    if (SamePersistentCharacter(identity_, next) && !force_session_gap) {
        if (!next.display_name.empty()) {
            identity_.display_name = next.display_name;
        }
        if (!next.profession.empty()) {
            identity_.profession = next.profession;
        }
        if (next.is_pre_searing.has_value()) {
            identity_.is_pre_searing = next.is_pre_searing;
        }
        ApplyIdentityMetadataBackfill();
        return;
    }

    // Stable ephemeral session: metadata only — do not clear progress or raise a snapshot barrier.
    if (identity_.kind == IdentityKind::Ephemeral && next.kind == IdentityKind::Ephemeral
        && SameAccount(identity_, next) && !force_session_gap) {
        if (!next.display_name.empty()) {
            identity_.display_name = next.display_name;
            character_.display_name = next.display_name;
        }
        if (!next.profession.empty()) {
            identity_.profession = next.profession;
        }
        if (next.is_pre_searing.has_value()) {
            identity_.is_pre_searing = next.is_pre_searing;
        }
        return;
    }

    const bool leaving_bound = identity_.kind != IdentityKind::Unbound;
    if (leaving_bound) {
        FinalizeOutgoingEvidence();
        if (identity_.kind == IdentityKind::Persistent
            && (semantic_dirty_ || heartbeat_pending_)) {
            if (!Flush(true)) {
                RetainActiveAsDetached(
                    has_persist_status_ ? last_persist_status_ : StoreOpStatus::IoError);
            }
            else {
                // Successful flush: still drop outgoing char if same-account bind follows? Disk has it.
                // Active store may still hold it until account reload; erase to avoid stale republish
                // only when we also had retained — on success disk is authoritative.
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
    snapshot_barrier_revision_ = reject_revision_at_or_below;
    awaiting_post_bind_snapshot_ = require_fresh_identity_snapshot;

    char gen_buf[96];
    std::snprintf(
        gen_buf, sizeof(gen_buf),
        "identity bind gen_a=%llu gen_c=%llu kind=%u barrier_rev=%llu await_fresh=%d",
        static_cast<unsigned long long>(account_generation_),
        static_cast<unsigned long long>(character_generation_),
        static_cast<unsigned>(identity_.kind),
        static_cast<unsigned long long>(snapshot_barrier_revision_),
        awaiting_post_bind_snapshot_ ? 1 : 0);
    AddDiag(gen_buf);

    if (identity_.kind == IdentityKind::Unbound) {
        account_store_ = {};
        load_status_ = StoreOpStatus::Empty;
        persistence_allowed_ = false;
        ResetActivePersistLatch();
        awaiting_post_bind_snapshot_ = false;
        AddDiag("identity unbound");
        return;
    }

    if (identity_.kind == IdentityKind::Ephemeral) {
        account_store_ = {};
        load_status_ = StoreOpStatus::Empty;
        persistence_allowed_ = false;
        character_.character_key.clear();
        if (!identity_.display_name.empty()) {
            character_.display_name = identity_.display_name;
        }
        ResetActivePersistLatch();
        AddDiag("ephemeral identity; no persistent file I/O");
        return;
    }

    if (account_changed || account_store_.account_key != identity_.account_key) {
        LoadAccountForIdentity();
    }
    else {
        if (load_status_ == StoreOpStatus::CodecError
            || load_status_ == StoreOpStatus::UnsupportedDiskMajor) {
            persistence_allowed_ = false;
            persist_latch_ = PersistLatch::BlockedPermanent;
        }
        else if (load_status_ == StoreOpStatus::MergeConflict) {
            persistence_allowed_ = false;
            persist_latch_ = PersistLatch::NeedsIntervention;
        }
        else if (!persistence_allowed_
                 && !IsPermanentBlockStatus(load_status_)
                 && !IsInterventionStatus(load_status_)) {
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
    ApplyIdentityMetadataBackfill();
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
    snapshot_barrier_revision_ = 0;
    awaiting_post_bind_snapshot_ = false;
    ResetActivePersistLatch();
    AddDiag("explicit logout unbound");
}

bool QuestProgressService::ApplyEvidenceToCharacter(
    CharacterProgress& target_character,
    SessionIdentity& identity_meta,
    std::vector<EvidenceStamp>& pending,
    uint64_t& dirty_generation,
    bool& semantic_dirty,
    bool& heartbeat_pending,
    PersistLatch& latch,
    std::chrono::system_clock::time_point wall_now,
    std::chrono::steady_clock::time_point steady_now)
{
    if (pending.empty()) {
        return false;
    }
    const auto observed_at = FormatCanonicalUtc(wall_now);
    if (!IsCanonicalUtcTimestamp(observed_at)) {
        AddDiag("refusing evidence finalize: non-canonical timestamp");
        return false;
    }

    auto evidence = CollectEvidenceFrom(pending, steady_now);
    if (evidence.empty()) {
        return false;
    }

    std::unordered_set<uint32_t> actionable;
    for (const auto& e : evidence) {
        if (e.kind == EvidenceKind::Abandon || e.kind == EvidenceKind::Reward || e.kind == EvidenceKind::ChatReward) {
            actionable.insert(e.game_quest_id);
        }
    }
    if (actionable.empty()) {
        return false;
    }

    ReducerInput input;
    input.previous = target_character;
    input.observed_at_utc = observed_at;
    input.session_gap = false;
    input.treat_as_stale_if_older = true;
    input.evidence = evidence;
    for (const auto& [quest_id, quest] : target_character.quests) {
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
    target_character = out.next;
    target_character.character_key = identity_meta.character_key;
    target_character.display_name = identity_meta.display_name;
    for (const auto& m : out.diagnostics.messages) {
        AddDiag(m);
    }
    if (out.semantic_changed) {
        ++dirty_generation;
        semantic_dirty = true;
        if (!LatchBlocksAutoRetry(latch)) {
            latch = PersistLatch::DirtyDebouncing;
        }
    }
    else if (out.touch_last_observed) {
        heartbeat_pending = true;
    }

    pending.erase(
        std::remove_if(
            pending.begin(), pending.end(),
            [&](const EvidenceStamp& e) { return actionable.count(e.game_quest_id) != 0; }),
        pending.end());
    return out.semantic_changed || out.touch_last_observed;
}

void QuestProgressService::FinalizeOutgoingEvidence()
{
    if (identity_.kind == IdentityKind::Unbound || !has_reduced_once_ || pending_evidence_.empty()) {
        return;
    }
    const auto wall_now = std::chrono::system_clock::now();
    const auto steady_now = std::chrono::steady_clock::now();
    last_tick_steady_ = steady_now;
    if (ApplyEvidenceToCharacter(
            character_, identity_, pending_evidence_, dirty_generation_, semantic_dirty_,
            heartbeat_pending_, persist_latch_, wall_now, steady_now)) {
        SyncCharacterIntoAccountStore();
        AddDiag("finalized outgoing scoped evidence");
    }
}

void QuestProgressService::FinalizeDetachedEvidence(
    DetachedDirtySession& detached, std::chrono::steady_clock::time_point steady_now)
{
    if (detached.pending_evidence.empty()) {
        return;
    }
    const auto wall_now = std::chrono::system_clock::now();
    if (ApplyEvidenceToCharacter(
            detached.character, detached.identity, detached.pending_evidence,
            detached.dirty_generation, detached.semantic_dirty, detached.heartbeat_pending,
            detached.latch, wall_now, steady_now)) {
        if (auto* stored = FindCharacter(detached.account_store, detached.key.character_key)) {
            stored->quests = detached.character.quests;
            stored->last_observed_at = detached.character.last_reduced_at;
            stored->display_name = detached.character.display_name;
        }
        AddDiag("finalized detached scoped evidence");
    }
    (void)steady_now;
}

void QuestProgressService::RouteOrDropEvidence(EvidenceStamp stamp)
{
    if (stamp.game_quest_id == 0 || IsSyntheticQuestId(stamp.game_quest_id)
        || stamp.kind == EvidenceKind::None) {
        return;
    }

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

    // Route by durable keys so coalesced sessions still accept late evidence.
    for (auto& [key, detached] : detached_sessions_) {
        const bool key_match = !stamp.account_key.empty() && !stamp.character_key.empty()
            && stamp.account_key == key.account_key && stamp.character_key == key.character_key;
        const bool gen_match = stamp.account_generation == detached.account_generation
            && stamp.character_generation == detached.character_generation;
        if (key_match || gen_match) {
            detached.pending_evidence.push_back(std::move(stamp));
            FinalizeDetachedEvidence(detached, last_tick_steady_);
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
    const auto expired = [&](const EvidenceStamp& e) {
        return steady_now - e.steady_at > kEvidencePairingWindow;
    };
    const auto before = pending_evidence_.size();
    pending_evidence_.erase(
        std::remove_if(pending_evidence_.begin(), pending_evidence_.end(), expired),
        pending_evidence_.end());
    if (pending_evidence_.size() != before) {
        AddDiag("expired unpaired active evidence");
    }

    for (auto& [key, detached] : detached_sessions_) {
        (void)key;
        const auto d_before = detached.pending_evidence.size();
        detached.pending_evidence.erase(
            std::remove_if(detached.pending_evidence.begin(), detached.pending_evidence.end(), expired),
            detached.pending_evidence.end());
        if (detached.pending_evidence.size() != d_before) {
            AddDiag("expired unpaired detached evidence");
        }
    }
}

std::vector<QuestEvidence> QuestProgressService::CollectEvidenceFrom(
    std::vector<EvidenceStamp>& pending,
    std::chrono::steady_clock::time_point steady_now)
{
    pending.erase(
        std::remove_if(
            pending.begin(), pending.end(),
            [&](const EvidenceStamp& e) {
                return steady_now - e.steady_at > kEvidencePairingWindow;
            }),
        pending.end());
    std::vector<QuestEvidence> out;
    out.reserve(pending.size());
    for (const auto& e : pending) {
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
    if (!SnapshotPassesIdentityBarrier(snap)) {
        AddDiag("rejected snapshot: identity mismatch, unstamped post-bind, or pre-bind revision");
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
    input.evidence = CollectEvidenceFrom(pending_evidence_, steady_now);

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
    if (!identity_.display_name.empty()) {
        character_.display_name = identity_.display_name;
    }
    else if (character_.display_name.empty()) {
        ApplyIdentityMetadataBackfill();
    }
    last_reduced_revision_ = snap.revision;
    has_reduced_once_ = true;
    awaiting_post_bind_snapshot_ = false;
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

bool QuestProgressService::SnapshotPassesIdentityBarrier(const QuestSnapshot& snap) const
{
    if (snap.revision <= snapshot_barrier_revision_) {
        return false;
    }

    if (snap.identity_captured) {
        if (snap.account_key != identity_.account_key
            || snap.character_key != identity_.character_key) {
            return false;
        }
        return true;
    }

    // Unstamped snaps are rejected while awaiting a post-bind identity-scoped observation.
    if (awaiting_post_bind_snapshot_) {
        return false;
    }
    return true;
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

void QuestProgressService::IngestMissionCompletion(std::map<uint32_t, MissionRecord> missions)
{
    if (!accept_input_ || missions.empty()) {
        return;
    }
    if (identity_.kind != IdentityKind::Persistent || identity_.character_key.empty()) {
        return;
    }
    EnsureCharacterRecord();
    auto* stored = FindCharacter(account_store_, identity_.character_key);
    if (!stored) {
        return;
    }

    if (!MergeMissionRecords(stored->missions, missions)) {
        return;
    }

    semantic_dirty_ = true;
    heartbeat_pending_ = false;
    if (!LatchBlocksAutoRetry(persist_latch_)) {
        persist_latch_ = PersistLatch::DirtyDebouncing;
    }
}

void QuestProgressService::IngestJourneySnapshot(JourneySnapshotResult snapshot)
{
    if (!accept_input_) {
        return;
    }
    if (identity_.kind != IdentityKind::Persistent || identity_.character_key.empty()) {
        return;
    }
    EnsureCharacterRecord();
    auto* stored = FindCharacter(account_store_, identity_.character_key);
    if (!stored) {
        return;
    }

    bool changed = false;
    if (snapshot.titles != stored->titles) {
        stored->titles = std::move(snapshot.titles);
        changed = true;
    }
    if (snapshot.level != stored->last_known_level) {
        stored->last_known_level = snapshot.level;
        changed = true;
    }
    const auto before = stored->journey_events.size();
    AppendUniqueJourneyEvents(stored->journey_events, snapshot.new_events);
    if (stored->journey_events.size() != before) {
        changed = true;
    }
    if (!changed) {
        return;
    }

    semantic_dirty_ = true;
    heartbeat_pending_ = false;
    if (!LatchBlocksAutoRetry(persist_latch_)) {
        persist_latch_ = PersistLatch::DirtyDebouncing;
    }
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
    if (LatchBlocksAutoRetry(persist_latch_)) {
        return false;
    }
    if (!persistence_allowed_ && identity_.kind == IdentityKind::Persistent) {
        if (IsPermanentBlockStatus(load_status_) || IsInterventionStatus(load_status_)) {
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
             && !LatchBlocksAutoRetry(persist_latch_)) {
        if (last_heartbeat_save_.time_since_epoch().count() == 0
            || (steady_now - last_heartbeat_save_) >= kHeartbeatInterval) {
            if (ShouldAttemptPersist(steady_now, false)) {
                TryPersist(false, steady_now);
            }
        }
    }
}

bool QuestProgressService::TryPersistDetached(
    DetachedSessionKey key, std::chrono::steady_clock::time_point steady_now)
{
    auto it = detached_sessions_.find(key);
    if (it == detached_sessions_.end()) {
        return false;
    }
    auto& detached = it->second;
    FinalizeDetachedEvidence(detached, steady_now);

    if (!detached.semantic_dirty && !detached.heartbeat_pending) {
        detached_sessions_.erase(it);
        return true;
    }
    if (LatchBlocksAutoRetry(detached.latch)) {
        return false;
    }
    if (detached.latch == PersistLatch::BlockedRetryable && steady_now < detached.next_retry_at) {
        return false;
    }
    if (detached.identity.kind != IdentityKind::Persistent || store_directory_.empty()) {
        return false;
    }

    ++persist_attempt_count_;
    detached.account_store.account_key = detached.key.account_key;
    detached.account_store.store_format = kStoreFormatId;
    detached.account_store.store_version = {kStoreFormatMajor, kStoreFormatMinor};
    if (auto* stored = FindCharacter(detached.account_store, detached.key.character_key)) {
        stored->quests = detached.character.quests;
        if (!detached.character.last_reduced_at.empty()) {
            stored->last_observed_at = detached.character.last_reduced_at;
        }
    }

    const uint64_t gen_at_start = detached.dirty_generation;
    const auto saved = SaveMergedAccountStore(
        store_directory_, detached.key.account_key, detached.account_store, lock_timeout_ms_);
    for (const auto& m : saved.diagnostics.messages) {
        AddDiag(m);
    }

    if (saved.status != StoreOpStatus::Ok || !saved.merged.has_value()) {
        NotePersistStatus(
            saved.status, "detached",
            detached.account_generation, detached.character_generation, detached.key.character_key);
        detached.last_status = saved.status;
        if (IsInterventionStatus(saved.status)) {
            detached.latch = PersistLatch::NeedsIntervention;
        }
        else if (IsPermanentBlockStatus(saved.status)) {
            detached.latch = PersistLatch::BlockedPermanent;
        }
        else if (IsRetryableBlockStatus(saved.status)) {
            detached.latch = PersistLatch::BlockedRetryable;
            detached.next_retry_at = steady_now + BackoffForAttempt(detached.retry_attempts);
            ++detached.retry_attempts;
        }
        else {
            detached.latch = PersistLatch::BlockedRetryable;
            detached.next_retry_at = steady_now + BackoffForAttempt(detached.retry_attempts);
            ++detached.retry_attempts;
        }
        return false;
    }

    if (detached.dirty_generation == gen_at_start) {
        detached.semantic_dirty = false;
        detached.heartbeat_pending = false;
    }
    ++successful_save_count_;
    AddDiag("detached session flush ok");
    detached_sessions_.erase(key);
    return true;
}

void QuestProgressService::TickDetachedRetries(std::chrono::steady_clock::time_point steady_now)
{
    std::vector<DetachedSessionKey> keys;
    keys.reserve(detached_sessions_.size());
    for (const auto& [key, detached] : detached_sessions_) {
        if (LatchBlocksAutoRetry(detached.latch)) {
            continue;
        }
        if (!detached.semantic_dirty && !detached.heartbeat_pending) {
            keys.push_back(key);
            continue;
        }
        if (detached.latch == PersistLatch::BlockedRetryable && steady_now < detached.next_retry_at) {
            continue;
        }
        keys.push_back(key);
    }
    for (const auto& key : keys) {
        auto it = detached_sessions_.find(key);
        if (it == detached_sessions_.end()) {
            continue;
        }
        if (!it->second.semantic_dirty && !it->second.heartbeat_pending) {
            detached_sessions_.erase(it);
            continue;
        }
        TryPersistDetached(key, steady_now);
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
        // Permanent/intervention: counter/diagnostic only on status transition via NotePersistStatus.
        if (LatchBlocksAutoRetry(persist_latch_) || IsPermanentBlockStatus(load_status_)
            || IsInterventionStatus(load_status_)) {
            NotePersistStatus(
                IsPermanentBlockStatus(load_status_) || IsInterventionStatus(load_status_)
                    ? load_status_
                    : last_persist_status_,
                "blocked",
                account_generation_, character_generation_, identity_.character_key);
        }
        return false;
    }

    ++persist_attempt_count_;

    if (!persistence_allowed_) {
        if (IsInterventionStatus(load_status_)) {
            persist_latch_ = PersistLatch::NeedsIntervention;
            NotePersistStatus(
                load_status_, "blocked",
                account_generation_, character_generation_, identity_.character_key);
        }
        else if (IsPermanentBlockStatus(load_status_)) {
            persist_latch_ = PersistLatch::BlockedPermanent;
            NotePersistStatus(
                load_status_, "blocked",
                account_generation_, character_generation_, identity_.character_key);
        }
        else {
            ScheduleRetryBackoff(steady_now);
            NotePersistStatus(
                load_status_, "blocked",
                account_generation_, character_generation_, identity_.character_key);
        }
        return false;
    }
    if (store_directory_.empty()) {
        persist_latch_ = PersistLatch::BlockedPermanent;
        NotePersistStatus(
            StoreOpStatus::ValidationError, "blocked",
            account_generation_, character_generation_, identity_.character_key);
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
        NotePersistStatus(
            saved.status, "save",
            account_generation_, character_generation_, identity_.character_key);
        if (IsInterventionStatus(saved.status)) {
            persist_latch_ = PersistLatch::NeedsIntervention;
            persistence_allowed_ = false;
        }
        else if (IsPermanentBlockStatus(saved.status)) {
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
