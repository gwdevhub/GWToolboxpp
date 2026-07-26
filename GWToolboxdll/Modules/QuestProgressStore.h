#pragma once

// Per-account quest progress store coordinator (Batch 2B). Offline-testable via explicit paths.
// Internal persistence only — not Contract v1 export. No GWCA / UI / lifecycle wiring.

#include <Modules/QuestProgressJsonCodec.h>

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace QuestProgress {

struct AccountStorePaths {
    std::filesystem::path directory;
    std::filesystem::path primary;
    std::filesystem::path backup;
    std::filesystem::path tmp;
};

enum class StoreOpStatus : uint8_t {
    Ok = 0,
    Empty,
    RecoveredFromBak,
    LockTimeout,
    LockFailed,
    UnsupportedDiskMajor,
    MergeConflict,
    CodecError,
    IoError,
    ValidationError,
};

enum class WaitAcquireKind : uint8_t {
    Acquired = 0,
    Abandoned,
    Timeout,
    Failed,
};

struct StoreDiagnostics {
    std::vector<std::string> messages;
    CodecDiagnostics codec;
    unsigned long win_error = 0;
    bool abandoned_lock = false;
    bool dirty_retained = false;
};

struct LoadStoreResult {
    StoreOpStatus status = StoreOpStatus::IoError;
    AccountProgressStore store;
    StoreDiagnostics diagnostics;
};

struct SaveStoreResult {
    StoreOpStatus status = StoreOpStatus::IoError;
    std::optional<AccountProgressStore> merged;
    StoreDiagnostics diagnostics;
    bool used_move_file_ex = false;
    bool used_replace_file = false;
};

// merged is engaged only when status == Ok; conflict/error leaves it disengaged.
struct MergeStoreResult {
    StoreOpStatus status = StoreOpStatus::MergeConflict;
    std::optional<AccountProgressStore> merged;
    StoreDiagnostics diagnostics;
};

// Path helpers (no I/O). account_key must already be NormalizeAccountKey()'d.
AccountStorePaths BuildAccountStorePaths(
    const std::filesystem::path& quest_progress_dir,
    std::string_view normalized_account_key);

// Mutex: Local\GWToolbox.QuestProgress.<16-hex FNV-1a-64 of UTF-8 account key>
std::string BuildAccountMutexName(std::string_view normalized_account_key);

// Testable classification of WaitForSingleObject results (do not use GetLastError for abandoned).
WaitAcquireKind ClassifyWaitResult(unsigned long wait_result);

MergeStoreResult MergeAccountStores(
    const AccountProgressStore& disk,
    const AccountProgressStore& memory);

// Coalesce two StoredCharacter snapshots for the same characterKey (detached-session union).
// On semanticEventKey payload conflict: status=MergeConflict, character still holds a
// preferred projection + unioned non-conflicting history; conflict_variants retains
// alternate payloads (deduped). Caller must not auto-retry unchanged conflicts.
struct CoalesceCharacterResult {
    StoreOpStatus status = StoreOpStatus::Ok;
    StoredCharacter character;
    // semantic_event_key → alternate payloads not selected as the canonical history row
    std::map<std::string, std::vector<QuestHistoryEvent>> conflict_variants;
    StoreDiagnostics diagnostics;
};

CoalesceCharacterResult CoalesceStoredCharacters(
    const StoredCharacter& existing,
    const StoredCharacter& incoming);

// Missing primary+backup → Empty. Existing empty/whitespace/malformed primary → try .bak;
// unusable primary+bak → CodecError (never treated as Empty). Never deletes files.
LoadStoreResult LoadAccountStore(
    const std::filesystem::path& quest_progress_dir,
    std::string_view account_key);

// Under mutex: re-read, merge memory into disk, atomic write. Timeout retains dirty (caller keeps memory).
SaveStoreResult SaveMergedAccountStore(
    const std::filesystem::path& quest_progress_dir,
    std::string_view account_key,
    const AccountProgressStore& memory_store,
    unsigned long lock_timeout_ms = 2000);

} // namespace QuestProgress
