#pragma once

// Runtime quest progress coordinator (Batch 2C). Offline-testable via explicit store paths.
// No Draw I/O. No Contract export.

#include <Modules/QuestProgressDomain.h>
#include <Modules/QuestProgressReducer.h>
#include <Modules/QuestProgressStore.h>
#include <Modules/QuestSessionIdentity.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace QuestProgress {

inline constexpr auto kEvidencePairingWindow = std::chrono::seconds(5);
inline constexpr auto kSemanticSaveDebounce = std::chrono::milliseconds(1000);
inline constexpr auto kHeartbeatInterval = std::chrono::minutes(5);
inline constexpr auto kPersistRetryInitialBackoff = std::chrono::milliseconds(500);
inline constexpr auto kPersistRetryMaxBackoff = std::chrono::seconds(30);
inline constexpr size_t kMaxDiagnostics = 100;

enum class PersistLatch : uint8_t {
    Clean = 0,
    DirtyDebouncing,
    ReadyToSave,
    Saving,
    BlockedPermanent,
    NeedsIntervention, // MergeConflict / coalesce payload conflict — no auto-retry
    BlockedRetryable,
    RetainedDetachedSession,
};

struct QuestSnapshotQuest {
    uint32_t game_quest_id = 0;
    bool in_log_completed = false;
    bool objectives_missing = false;
    std::vector<ObjectiveObservation> objectives;
};

struct QuestSnapshot {
    uint64_t revision = 0;
    bool loading = false;
    bool world_ready = false;
    // Owned identity captured with the quest log; never inferred after bind.
    bool identity_captured = false;
    std::string account_key;
    std::string character_key;
    // Transient UI selection — never passed into ReducerInput.
    uint32_t selected_active_quest_id = 0;
    std::vector<QuestSnapshotQuest> quests;
};

// Session scope is captured at enqueue/drain time — never inferred from a later bind.
struct EvidenceStamp {
    uint32_t game_quest_id = 0;
    EvidenceKind kind = EvidenceKind::None;
    std::chrono::steady_clock::time_point steady_at{};
    uint64_t account_generation = 0;
    uint64_t character_generation = 0;
    std::string account_key;
    std::string character_key;
};

// Phase-2 detached identity: accountKey + characterKey (never displayName).
struct DetachedSessionKey {
    std::string account_key;
    std::string character_key;

    bool operator<(const DetachedSessionKey& other) const
    {
        if (account_key != other.account_key) {
            return account_key < other.account_key;
        }
        return character_key < other.character_key;
    }

    bool operator==(const DetachedSessionKey& other) const
    {
        return account_key == other.account_key && character_key == other.character_key;
    }
};

// One keyed detached dirty session (minimal one-character account store).
struct DetachedDirtySession {
    DetachedSessionKey key{};
    SessionIdentity identity{};
    uint64_t account_generation = 0;
    uint64_t character_generation = 0;
    // Minimal store: format/version/account_key + exactly this character.
    AccountProgressStore account_store{};
    CharacterProgress character{};
    bool semantic_dirty = false;
    bool heartbeat_pending = false;
    uint64_t dirty_generation = 0;
    StoreOpStatus last_status = StoreOpStatus::IoError;
    PersistLatch latch = PersistLatch::RetainedDetachedSession;
    std::chrono::steady_clock::time_point next_retry_at{};
    uint32_t retry_attempts = 0;
    std::vector<EvidenceStamp> pending_evidence;
    // Alternate history payloads for the same semanticEventKey (coalesce conflicts).
    std::map<std::string, std::vector<QuestHistoryEvent>> conflict_variants;
};

std::string FormatCanonicalUtc(std::chrono::system_clock::time_point wall_now);

class QuestProgressService {
public:
    using PersistTestHook = std::function<void()>;

    void SetStoreDirectory(std::filesystem::path directory);
    const std::filesystem::path& store_directory() const { return store_directory_; }

    void SetPersistTestHook(PersistTestHook hook) { persist_test_hook_ = std::move(hook); }
    void SetLockTimeoutMs(unsigned long timeout_ms) { lock_timeout_ms_ = timeout_ms; }

    void Initialize();
    void SignalTerminate();
    void Terminate();

    void Tick(
        std::chrono::steady_clock::time_point steady_now,
        std::chrono::system_clock::time_point wall_now);

    // reject_revision_at_or_below: discard published snaps at/under this revision after a character bind.
    // require_fresh_identity_snapshot: reject unstamped/pre-bind snaps until a matching post-bind snap arrives.
    void BindIdentity(
        const SessionIdentity& identity,
        bool force_session_gap = false,
        uint64_t reject_revision_at_or_below = 0,
        bool require_fresh_identity_snapshot = false);
    void UnbindIdentity();
    void IngestEvidence(std::vector<EvidenceStamp> stamps);
    void IngestSnapshot(
        const QuestSnapshot& snap,
        std::chrono::system_clock::time_point wall_now,
        std::chrono::steady_clock::time_point steady_now);

    // Mission completion bitsets (mission_completion_data). Pure merge; GWCA sampling lives in QuestProgressLive.
    void IngestMissionCompletion(std::map<uint32_t, MissionRecord> missions);

    void IngestJourneySnapshot(JourneySnapshotResult snapshot);
    void IngestJourneyEvents(std::vector<JourneyEventRecord> events);
    void IngestHomSnapshot(HomSnapshotRecord snapshot);

    bool Flush(bool session_boundary);

    // Snapshot of the currently bound persistent character for Contract export (identity-scoped).
    std::optional<StoredCharacter> BuildExportCharacterSnapshot() const;

    const SessionIdentity& identity() const { return identity_; }
    const CharacterProgress& character_progress() const { return character_; }
    const AccountProgressStore& account_store() const { return account_store_; }
    StoreOpStatus load_status() const { return load_status_; }
    bool persistence_allowed() const { return persistence_allowed_; }
    bool semantic_dirty() const { return semantic_dirty_; }
    bool heartbeat_pending() const { return heartbeat_pending_; }
    bool initialized() const { return initialized_; }
    bool terminate_signaled() const { return terminate_signaled_; }
    const std::vector<std::string>& diagnostics() const { return diagnostics_; }
    uint64_t last_reduced_revision() const { return last_reduced_revision_; }
    uint64_t snapshot_barrier_revision() const { return snapshot_barrier_revision_; }
    bool awaiting_post_bind_snapshot() const { return awaiting_post_bind_snapshot_; }
    size_t successful_save_count() const { return successful_save_count_; }
    size_t blocked_save_count() const { return blocked_save_count_; }
    bool last_flush_ok() const { return last_flush_ok_; }
    uint64_t account_generation() const { return account_generation_; }
    uint64_t character_generation() const { return character_generation_; }
    uint64_t dirty_generation() const { return dirty_generation_; }
    PersistLatch persist_latch() const { return persist_latch_; }
    size_t detached_session_count() const { return detached_sessions_.size(); }
    const std::map<DetachedSessionKey, DetachedDirtySession>& detached_sessions() const
    {
        return detached_sessions_;
    }
    size_t persist_attempt_count() const { return persist_attempt_count_; }
    std::chrono::steady_clock::time_point next_persist_retry_at() const { return next_persist_retry_at_; }

private:
    void AddDiag(std::string message);
    void NotePersistStatus(
        StoreOpStatus status,
        const char* operation,
        uint64_t scope_account_gen,
        uint64_t scope_character_gen,
        std::string_view scope_character_key);
    void SyncCharacterIntoAccountStore();
    void LoadAccountForIdentity();
    void EnsureCharacterRecord();
    void ClearActiveSessionMemory();
    void RetainActiveAsDetached(StoreOpStatus last_status);
    void RemoveOutgoingCharacterFromActiveStore();
    StoredCharacter BuildOutgoingStoredCharacter() const;
    AccountProgressStore BuildMinimalAccountStore(const StoredCharacter& character) const;
    void ResetActivePersistLatch();
    void MarkSemanticDirty(std::chrono::steady_clock::time_point steady_now);
    void RouteOrDropEvidence(EvidenceStamp stamp);
    bool ApplyEvidenceToCharacter(
        CharacterProgress& character,
        SessionIdentity& identity_meta,
        std::vector<EvidenceStamp>& pending,
        uint64_t& dirty_generation,
        bool& semantic_dirty,
        bool& heartbeat_pending,
        PersistLatch& latch,
        std::chrono::system_clock::time_point wall_now,
        std::chrono::steady_clock::time_point steady_now);
    void FinalizeOutgoingEvidence();
    void FinalizeDetachedEvidence(DetachedDirtySession& detached, std::chrono::steady_clock::time_point steady_now);
    std::vector<QuestEvidence> CollectEvidenceFrom(
        std::vector<EvidenceStamp>& pending,
        std::chrono::steady_clock::time_point steady_now);
    void ExpireEvidence(std::chrono::steady_clock::time_point steady_now);
    bool SnapshotPassesIdentityBarrier(const QuestSnapshot& snap) const;
    void ApplyIdentityMetadataBackfill();
    void ReduceFromSnapshot(
        const QuestSnapshot& snap,
        std::chrono::system_clock::time_point wall_now,
        std::chrono::steady_clock::time_point steady_now);
    bool TryPersist(bool session_boundary);
    bool TryPersist(bool session_boundary, std::chrono::steady_clock::time_point steady_now);
    bool TryPersistDetached(DetachedSessionKey key, std::chrono::steady_clock::time_point steady_now);
    void TickDetachedRetries(std::chrono::steady_clock::time_point steady_now);
    bool ShouldAttemptPersist(std::chrono::steady_clock::time_point steady_now, bool session_boundary) const;
    void ScheduleRetryBackoff(std::chrono::steady_clock::time_point steady_now);
    static bool IsPermanentBlockStatus(StoreOpStatus status);
    static bool IsRetryableBlockStatus(StoreOpStatus status);
    static bool IsInterventionStatus(StoreOpStatus status);
    static DetachedSessionKey MakeDetachedKey(const SessionIdentity& identity);

    std::filesystem::path store_directory_;
    bool initialized_ = false;
    bool terminate_signaled_ = false;
    bool accept_input_ = true;
    bool final_flush_requested_ = false;

    SessionIdentity identity_{};
    uint64_t account_generation_ = 0;
    uint64_t character_generation_ = 0;
    AccountProgressStore account_store_{};
    CharacterProgress character_{};
    StoreOpStatus load_status_ = StoreOpStatus::Empty;
    bool persistence_allowed_ = false;
    bool session_gap_pending_ = false;

    std::vector<EvidenceStamp> pending_evidence_;
    uint64_t last_reduced_revision_ = 0;
    uint64_t snapshot_barrier_revision_ = 0;
    bool awaiting_post_bind_snapshot_ = false;
    bool has_reduced_once_ = false;

    bool semantic_dirty_ = false;
    bool heartbeat_pending_ = false;
    uint64_t dirty_generation_ = 0;
    PersistLatch persist_latch_ = PersistLatch::Clean;
    StoreOpStatus last_persist_status_ = StoreOpStatus::Ok;
    bool has_persist_status_ = false;
    std::chrono::steady_clock::time_point next_persist_retry_at_{};
    uint32_t persist_retry_attempts_ = 0;
    std::chrono::steady_clock::time_point last_semantic_change_{};
    std::chrono::steady_clock::time_point last_heartbeat_save_{};
    std::chrono::steady_clock::time_point last_tick_steady_{};

    std::map<DetachedSessionKey, DetachedDirtySession> detached_sessions_;

    std::vector<std::string> diagnostics_;
    size_t successful_save_count_ = 0;
    size_t blocked_save_count_ = 0;
    size_t persist_attempt_count_ = 0;
    bool last_flush_ok_ = false;

    PersistTestHook persist_test_hook_;
    unsigned long lock_timeout_ms_ = 2000;
};

} // namespace QuestProgress
