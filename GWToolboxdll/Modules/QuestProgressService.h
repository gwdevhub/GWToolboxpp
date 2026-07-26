#pragma once

// Runtime quest progress coordinator (Batch 2C). Offline-testable via explicit store paths.
// No Draw I/O. No Contract export. No mission-bit observation.

#include <Modules/QuestProgressDomain.h>
#include <Modules/QuestProgressReducer.h>
#include <Modules/QuestProgressStore.h>
#include <Modules/QuestSessionIdentity.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace QuestProgress {

inline constexpr auto kEvidencePairingWindow = std::chrono::seconds(5);
inline constexpr auto kSemanticSaveDebounce = std::chrono::milliseconds(1000);
inline constexpr auto kHeartbeatInterval = std::chrono::minutes(5);

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
    // Transient UI selection — never passed into ReducerInput.
    uint32_t selected_active_quest_id = 0;
    std::vector<QuestSnapshotQuest> quests;
};

struct EvidenceStamp {
    uint32_t game_quest_id = 0;
    EvidenceKind kind = EvidenceKind::None;
    std::chrono::steady_clock::time_point steady_at{};
};

// Canonical UTC timestamp for reducer input: YYYY-MM-DDTHH:MM:SS.sssZ
std::string FormatCanonicalUtc(std::chrono::system_clock::time_point wall_now);

class QuestProgressService {
public:
    void SetStoreDirectory(std::filesystem::path directory);
    const std::filesystem::path& store_directory() const { return store_directory_; }

    void Initialize();
    void SignalTerminate();
    void Terminate();

    // Drive identity/snapshot/evidence then scheduling. Call after adapters feed data.
    void Tick(
        std::chrono::steady_clock::time_point steady_now,
        std::chrono::system_clock::time_point wall_now);

    void BindIdentity(const SessionIdentity& identity, bool force_session_gap = false);
    void IngestEvidence(std::vector<EvidenceStamp> stamps);
    void IngestSnapshot(
        const QuestSnapshot& snap,
        std::chrono::system_clock::time_point wall_now,
        std::chrono::steady_clock::time_point steady_now);

    // session_boundary=true bypasses debounce (character switch / terminate).
    bool Flush(bool session_boundary);

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
    size_t successful_save_count() const { return successful_save_count_; }
    size_t blocked_save_count() const { return blocked_save_count_; }
    bool last_flush_ok() const { return last_flush_ok_; }

private:
    void AddDiag(std::string message);
    void SyncCharacterIntoAccountStore();
    void LoadAccountForIdentity();
    void EnsureCharacterRecord();
    std::vector<QuestEvidence> CollectActiveEvidence(std::chrono::steady_clock::time_point steady_now);
    void ExpireEvidence(std::chrono::steady_clock::time_point steady_now);
    void ReduceFromSnapshot(
        const QuestSnapshot& snap,
        std::chrono::system_clock::time_point wall_now,
        std::chrono::steady_clock::time_point steady_now);
    bool TryPersist(bool session_boundary);

    std::filesystem::path store_directory_;
    bool initialized_ = false;
    bool terminate_signaled_ = false;
    bool accept_input_ = true;
    bool final_flush_requested_ = false;

    SessionIdentity identity_{};
    AccountProgressStore account_store_{};
    CharacterProgress character_{};
    StoreOpStatus load_status_ = StoreOpStatus::Empty;
    bool persistence_allowed_ = false;
    bool session_gap_pending_ = false;

    std::vector<EvidenceStamp> pending_evidence_;
    uint64_t last_reduced_revision_ = 0;
    bool has_reduced_once_ = false;

    bool semantic_dirty_ = false;
    bool heartbeat_pending_ = false;
    std::chrono::steady_clock::time_point last_semantic_change_{};
    std::chrono::steady_clock::time_point last_heartbeat_save_{};
    std::chrono::steady_clock::time_point last_tick_steady_{};

    std::vector<std::string> diagnostics_;
    size_t successful_save_count_ = 0;
    size_t blocked_save_count_ = 0;
    bool last_flush_ok_ = false;
};

} // namespace QuestProgress
