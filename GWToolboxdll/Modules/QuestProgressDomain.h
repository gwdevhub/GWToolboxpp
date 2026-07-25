#pragma once

// Pure quest-progress domain types. No GWCA, ImGui, Windows, filesystem, or JSON.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace QuestProgress {

inline constexpr uint32_t kSyntheticCustomMarkerQuestId = 0x0000fdd;

enum class ProgressState : uint8_t {
    Unknown = 0,
    Available,           // reserved — Batch 2A reducer must never produce
    Active,
    ObjectiveProgress,
    ReadyForReward,
    CompletedObserved,
    CompletedManual,     // reserved — Batch 2A reducer must never produce
    AbandonedObserved,
};

enum class ProgressSource : uint8_t {
    GameSnapshot = 0,
    GameEvent,
    MissionCompletionData,
    ToolboxExistingData,
    ManualUserInput,
    ImportedHistory,
    Migration, // Contract Codex-internal; reducer must never produce
};

enum class Confidence : uint8_t {
    Confirmed = 0,
    Probable,
    Uncertain,
    Manual,
};

enum class HistoryEventType : uint8_t {
    Observation = 0,
    PresenceLost,
    Abandoned,
    Completed,
};

// Exact quest-id evidence already window-resolved by the caller.
enum class EvidenceKind : uint8_t {
    None = 0,
    Abandon,
    Reward,        // REWARD dialog turn-in
    EnquireReward, // never turn-in by itself
};

struct ObjectiveObservation {
    uint32_t index = 0;
    bool completed = false;
    // Owned encoded objective content (UTF-16 code units); used for fingerprinting only.
    std::u16string encoded_content;
    // Filled by NormalizeObjective / BuildSemanticEventKey helpers.
    std::string content_fingerprint;
};

struct QuestHistoryEvent {
    uint32_t game_quest_id = 0;
    HistoryEventType event_type = HistoryEventType::Observation;
    ProgressState state = ProgressState::Unknown;
    ProgressSource source = ProgressSource::GameSnapshot;
    Confidence confidence = Confidence::Uncertain;
    std::string observed_at; // UTC ISO-8601; not part of semanticEventKey
    std::string semantic_event_key;
    std::vector<ObjectiveObservation> objectives;
    EvidenceKind evidence_kind = EvidenceKind::None;
};

struct QuestProgress {
    uint32_t game_quest_id = 0;
    ProgressState state = ProgressState::Unknown;
    ProgressSource source = ProgressSource::GameSnapshot;
    Confidence confidence = Confidence::Uncertain;
    std::string first_observed_at;
    std::string last_observed_at;
    std::optional<std::string> completed_at;
    std::vector<ObjectiveObservation> objectives;
    std::vector<QuestHistoryEvent> history;
};

struct CharacterProgress {
    std::string character_key;
    std::string display_name; // metadata only; never identity
    // Ordered map keeps reduce/tests deterministic.
    std::map<uint32_t, QuestProgress> quests;
    std::string last_reduced_at;
};

struct LiveQuestObservation {
    uint32_t game_quest_id = 0;
    bool in_log_completed = false;
    std::vector<ObjectiveObservation> objectives;
    bool objectives_missing = false;
};

struct QuestEvidence {
    uint32_t game_quest_id = 0;
    EvidenceKind kind = EvidenceKind::None;
};

struct ReducerInput {
    CharacterProgress previous;
    std::vector<LiveQuestObservation> observed_quests;
    std::vector<QuestEvidence> evidence;
    std::string observed_at_utc;
    // Offline / session gap: missing quests become unknown without pairing.
    bool session_gap = false;
    // Stale: observed_at older than previous.last_reduced_at → no mutations.
    bool treat_as_stale_if_older = true;
};

struct ReducerDiagnostics {
    std::vector<std::string> messages;
    bool rejected_stale = false;
    bool reserved_state_attempt = false;
};

struct ReducerOutput {
    CharacterProgress next;
    std::vector<QuestHistoryEvent> appended;
    ReducerDiagnostics diagnostics;
    // True when progress/history semantic content changed (persistence-worthy).
    bool semantic_changed = false;
    // Timestamp-only refresh; not an immediate persistence-worthy semantic change.
    bool touch_last_observed = false;
};

const char* ToString(ProgressState state);
const char* ToString(ProgressSource source);
const char* ToString(Confidence confidence);
const char* ToString(HistoryEventType type);
const char* ToString(EvidenceKind kind);

bool IsReservedState(ProgressState state);
bool IsSyntheticQuestId(uint32_t game_quest_id);

// Normalize encoded content (strip trailing NULs) and return FNV-1a-64 hex fingerprint.
std::string FingerprintEncodedContent(std::u16string_view encoded);
void NormalizeObjectives(std::vector<ObjectiveObservation>& objectives);

// Deterministic semantic identity (timestamp excluded). Algorithm: UTF-8 canonical
// string → FNV-1a 64-bit → lowercase hex. Not cryptographic; collision assumption ~2^-64.
std::string BuildSemanticEventKey(
    uint32_t game_quest_id,
    HistoryEventType event_type,
    ProgressState state,
    ProgressSource source,
    Confidence confidence,
    const std::vector<ObjectiveObservation>& objectives,
    EvidenceKind evidence_kind);

ProgressState DerivePresentState(const LiveQuestObservation& obs);

} // namespace QuestProgress
