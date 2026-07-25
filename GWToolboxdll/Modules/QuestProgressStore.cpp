#include <Modules/QuestProgressStore.h>
#include <Utils/AtomicJsonFile.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <algorithm>

namespace QuestProgress {
namespace {

constexpr uint64_t kFnvOffset = 14695981039346656037ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

uint64_t Fnv1a64(std::string_view s)
{
    uint64_t hash = kFnvOffset;
    for (unsigned char c : s) {
        hash ^= c;
        hash *= kFnvPrime;
    }
    return hash;
}

std::string ToHex64(uint64_t value)
{
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) {
        out[static_cast<size_t>(i)] = kHex[value & 0xfu];
        value >>= 4;
    }
    return out;
}

void AddDiag(StoreDiagnostics& d, std::string message)
{
    d.messages.push_back(std::move(message));
}

bool HistoryPayloadEqual(const QuestHistoryEvent& a, const QuestHistoryEvent& b)
{
    if (a.game_quest_id != b.game_quest_id
        || a.event_type != b.event_type
        || a.state != b.state
        || a.source != b.source
        || a.confidence != b.confidence
        || a.evidence_kind != b.evidence_kind
        || a.objectives.size() != b.objectives.size()) {
        return false;
    }
    for (size_t i = 0; i < a.objectives.size(); ++i) {
        if (a.objectives[i].index != b.objectives[i].index
            || a.objectives[i].completed != b.objectives[i].completed
            || a.objectives[i].content_fingerprint != b.objectives[i].content_fingerprint
            || a.objectives[i].encoded_content != b.objectives[i].encoded_content) {
            return false;
        }
    }
    return true;
}

bool PreferMemoryProjection(const QuestProgress& disk, const QuestProgress& memory)
{
    if (memory.last_observed_at != disk.last_observed_at) {
        return memory.last_observed_at > disk.last_observed_at;
    }
    // Same timestamp: deterministic tie-break via latest history semantic key.
    const auto disk_key = disk.history.empty() ? std::string{} : disk.history.back().semantic_event_key;
    const auto mem_key = memory.history.empty() ? std::string{} : memory.history.back().semantic_event_key;
    if (mem_key != disk_key) {
        return mem_key > disk_key;
    }
    return false; // stable: keep disk on total tie
}

QuestProgress ProjectQuest(const QuestProgress& disk, const QuestProgress& memory, StoreDiagnostics& d, bool& conflict)
{
    QuestProgress out = PreferMemoryProjection(disk, memory) ? memory : disk;

    // Union history by semanticEventKey.
    std::map<std::string, QuestHistoryEvent> by_key;
    auto ingest = [&](const QuestHistoryEvent& ev) {
        auto it = by_key.find(ev.semantic_event_key);
        if (it == by_key.end()) {
            by_key.emplace(ev.semantic_event_key, ev);
            return;
        }
        if (!HistoryPayloadEqual(it->second, ev)) {
            conflict = true;
            AddDiag(d, "history semanticEventKey payload conflict: " + ev.semantic_event_key);
        }
    };
    for (const auto& ev : disk.history) {
        ingest(ev);
    }
    for (const auto& ev : memory.history) {
        ingest(ev);
    }
    out.history.clear();
    for (auto& [k, ev] : by_key) {
        (void)k;
        out.history.push_back(std::move(ev));
    }
    std::sort(out.history.begin(), out.history.end(),
        [](const QuestHistoryEvent& a, const QuestHistoryEvent& b) {
            if (a.observed_at != b.observed_at) {
                return a.observed_at < b.observed_at;
            }
            return a.semantic_event_key < b.semantic_event_key;
        });

    // Projection fields from preferred side; first_observed_at = min of both when both set.
    const QuestProgress& preferred = PreferMemoryProjection(disk, memory) ? memory : disk;
    out.state = preferred.state;
    out.source = preferred.source;
    out.confidence = preferred.confidence;
    out.objectives = preferred.objectives;
    out.completed_at = preferred.completed_at;
    out.last_observed_at = preferred.last_observed_at;
    out.game_quest_id = preferred.game_quest_id;
    if (disk.first_observed_at.empty()) {
        out.first_observed_at = memory.first_observed_at;
    }
    else if (memory.first_observed_at.empty()) {
        out.first_observed_at = disk.first_observed_at;
    }
    else {
        out.first_observed_at = std::min(disk.first_observed_at, memory.first_observed_at);
    }
    NormalizeObjectives(out.objectives);
    return out;
}

MissionRecord ProjectMission(const MissionRecord& disk, const MissionRecord& memory)
{
    if (memory.last_observed_at != disk.last_observed_at) {
        return memory.last_observed_at > disk.last_observed_at ? memory : disk;
    }
    // Tie: OR completion flags, prefer memory map_id (same).
    MissionRecord out = disk;
    out.completed_normal = disk.completed_normal || memory.completed_normal;
    out.completed_hard = disk.completed_hard || memory.completed_hard;
    out.bonus_normal = disk.bonus_normal || memory.bonus_normal;
    out.bonus_hard = disk.bonus_hard || memory.bonus_hard;
    return out;
}

StoredCharacter MergeCharacter(const StoredCharacter& disk, const StoredCharacter& memory, StoreDiagnostics& d, bool& conflict)
{
    StoredCharacter out;
    out.character_key = disk.character_key.empty() ? memory.character_key : disk.character_key;

    const bool mem_newer = memory.last_observed_at > disk.last_observed_at
        || (memory.last_observed_at == disk.last_observed_at
            && memory.display_name >= disk.display_name);
    const StoredCharacter& meta = mem_newer ? memory : disk;
    out.display_name = meta.display_name;
    out.profession = meta.profession;
    out.is_pre_searing = meta.is_pre_searing;
    out.last_observed_at = meta.last_observed_at;
    if (disk.first_observed_at.empty()) {
        out.first_observed_at = memory.first_observed_at;
    }
    else if (memory.first_observed_at.empty()) {
        out.first_observed_at = disk.first_observed_at;
    }
    else {
        out.first_observed_at = std::min(disk.first_observed_at, memory.first_observed_at);
    }

    std::map<uint32_t, char> ids;
    for (const auto& [id, _] : disk.quests) {
        (void)_;
        ids[id] = 1;
    }
    for (const auto& [id, _] : memory.quests) {
        (void)_;
        ids[id] = 1;
    }
    for (const auto& [id, _] : ids) {
        (void)_;
        const auto d_it = disk.quests.find(id);
        const auto m_it = memory.quests.find(id);
        if (d_it == disk.quests.end()) {
            out.quests.emplace(id, m_it->second);
        }
        else if (m_it == memory.quests.end()) {
            out.quests.emplace(id, d_it->second);
        }
        else {
            out.quests.emplace(id, ProjectQuest(d_it->second, m_it->second, d, conflict));
        }
    }

    std::map<uint32_t, char> maps;
    for (const auto& [id, _] : disk.missions) {
        (void)_;
        maps[id] = 1;
    }
    for (const auto& [id, _] : memory.missions) {
        (void)_;
        maps[id] = 1;
    }
    for (const auto& [id, _] : maps) {
        (void)_;
        const auto d_it = disk.missions.find(id);
        const auto m_it = memory.missions.find(id);
        if (d_it == disk.missions.end()) {
            out.missions.emplace(id, m_it->second);
        }
        else if (m_it == memory.missions.end()) {
            out.missions.emplace(id, d_it->second);
        }
        else {
            out.missions.emplace(id, ProjectMission(d_it->second, m_it->second));
        }
    }
    return out;
}

class AccountMutex {
public:
    explicit AccountMutex(std::string mutex_name)
        : name_(std::move(mutex_name))
    {
        // CreateMutexW returns handle; preexisting is OK.
        const std::wstring wide(name_.begin(), name_.end());
        handle_ = CreateMutexW(nullptr, FALSE, wide.c_str());
        if (!handle_) {
            create_error_ = GetLastError();
        }
    }

    ~AccountMutex()
    {
        if (owned_ && handle_) {
            ReleaseMutex(handle_);
        }
        if (handle_) {
            CloseHandle(handle_);
        }
    }

    AccountMutex(const AccountMutex&) = delete;
    AccountMutex& operator=(const AccountMutex&) = delete;

    WaitAcquireKind Acquire(DWORD timeout_ms)
    {
        if (!handle_) {
            return WaitAcquireKind::Failed;
        }
        const auto wait = WaitForSingleObject(handle_, timeout_ms);
        const auto kind = ClassifyWaitResult(wait);
        if (kind == WaitAcquireKind::Acquired || kind == WaitAcquireKind::Abandoned) {
            owned_ = true;
        }
        if (kind == WaitAcquireKind::Failed) {
            create_error_ = GetLastError();
        }
        return kind;
    }

    DWORD create_error() const { return create_error_; }

private:
    std::string name_;
    HANDLE handle_ = nullptr;
    bool owned_ = false;
    DWORD create_error_ = 0;
};

LoadStoreResult LoadFromPaths(const AccountStorePaths& paths, std::string_view account_key)
{
    LoadStoreResult result;
    const auto normalized = NormalizeAccountKey(account_key);
    if (normalized.empty()) {
        result.status = StoreOpStatus::ValidationError;
        AddDiag(result.diagnostics, "invalid account key");
        return result;
    }

    auto try_parse = [&](std::string_view utf8, bool from_bak) -> bool {
        auto parsed = ParseAccountStoreJson(utf8, normalized);
        result.diagnostics.codec = parsed.diagnostics;
        if (parsed.status == CodecStatus::Ok) {
            result.store = std::move(parsed.store);
            result.status = from_bak ? StoreOpStatus::RecoveredFromBak : StoreOpStatus::Ok;
            if (from_bak) {
                result.diagnostics.codec.recovered_from_bak = true;
                AddDiag(result.diagnostics, "recovered from .bak");
            }
            return true;
        }
        if (parsed.status == CodecStatus::UnsupportedNewerMajor) {
            result.status = StoreOpStatus::UnsupportedDiskMajor;
            AddDiag(result.diagnostics, "unsupported newer major on disk");
            return true; // handled terminal
        }
        result.status = StoreOpStatus::CodecError;
        return false;
    };

    const auto primary = AtomicJson::ReadFileUtf8(paths.primary);
    if (primary.missing) {
        result.status = StoreOpStatus::Empty;
        result.store.account_key = normalized;
        result.store.store_format = kStoreFormatId;
        result.store.store_version = {kStoreFormatMajor, kStoreFormatMinor};
        AddDiag(result.diagnostics, "primary missing; empty store");
        return result;
    }
    if (primary.ok && !primary.empty && try_parse(primary.utf8, false)) {
        return result;
    }

    // Empty or malformed primary → try .bak; do not delete either.
    if (primary.empty) {
        AddDiag(result.diagnostics, "primary empty; attempting .bak");
    }
    else if (!primary.ok) {
        AddDiag(result.diagnostics, "primary read failed; attempting .bak");
    }
    else {
        AddDiag(result.diagnostics, "primary malformed; attempting .bak");
    }

    const auto bak = AtomicJson::ReadFileUtf8(paths.backup);
    if (bak.ok && !bak.missing && !bak.empty && try_parse(bak.utf8, true)) {
        return result;
    }

    if (primary.missing == false && (primary.empty || !primary.ok || result.status == StoreOpStatus::CodecError)
        && (bak.missing || bak.empty || !bak.ok)) {
        result.status = StoreOpStatus::CodecError;
        AddDiag(result.diagnostics, "primary and .bak unusable; files preserved");
        return result;
    }

    // Empty primary, no bak → empty store (documented policy).
    if (primary.empty && (bak.missing || bak.empty)) {
        result.status = StoreOpStatus::Empty;
        result.store.account_key = normalized;
        result.store.store_format = kStoreFormatId;
        result.store.store_version = {kStoreFormatMajor, kStoreFormatMinor};
        AddDiag(result.diagnostics, "empty primary and no bak; empty store");
        return result;
    }

    result.status = StoreOpStatus::CodecError;
    AddDiag(result.diagnostics, "load failed; files preserved");
    return result;
}

} // namespace

AccountStorePaths BuildAccountStorePaths(
    const std::filesystem::path& quest_progress_dir,
    std::string_view normalized_account_key)
{
    AccountStorePaths paths;
    paths.directory = quest_progress_dir;
    const auto file = std::string(normalized_account_key) + ".json";
    paths.primary = quest_progress_dir / file;
    paths.backup = quest_progress_dir / (file + ".bak");
    paths.tmp = quest_progress_dir / (file + ".tmp");
    return paths;
}

std::string BuildAccountMutexName(std::string_view normalized_account_key)
{
    return std::string("Local\\GWToolbox.QuestProgress.") + ToHex64(Fnv1a64(normalized_account_key));
}

WaitAcquireKind ClassifyWaitResult(unsigned long wait_result)
{
    switch (wait_result) {
        case WAIT_OBJECT_0:
            return WaitAcquireKind::Acquired;
        case WAIT_ABANDONED:
            return WaitAcquireKind::Abandoned;
        case WAIT_TIMEOUT:
            return WaitAcquireKind::Timeout;
        default:
            return WaitAcquireKind::Failed;
    }
}

MergeStoreResult MergeAccountStores(
    const AccountProgressStore& disk,
    const AccountProgressStore& memory)
{
    MergeStoreResult result;
    if (!disk.account_key.empty() && !memory.account_key.empty()
        && disk.account_key != memory.account_key) {
        result.status = StoreOpStatus::ValidationError;
        AddDiag(result.diagnostics, "merge accountKey mismatch");
        return result;
    }
    if (disk.store_version.major > kStoreFormatMajor
        || (disk.store_version.major == kStoreFormatMajor
            && disk.store_version.minor > kStoreFormatMinor)) {
        result.status = StoreOpStatus::UnsupportedDiskMajor;
        AddDiag(result.diagnostics, "disk store newer than supported; refusing merge overwrite");
        return result;
    }

    bool conflict = false;
    AccountProgressStore out;
    out.store_format = kStoreFormatId;
    out.store_version = {kStoreFormatMajor, kStoreFormatMinor};
    out.account_key = !memory.account_key.empty() ? memory.account_key : disk.account_key;

    std::map<std::string, char> keys;
    for (const auto& [k, _] : disk.characters) {
        (void)_;
        keys[k] = 1;
    }
    for (const auto& [k, _] : memory.characters) {
        (void)_;
        keys[k] = 1;
    }
    for (const auto& [key, _] : keys) {
        (void)_;
        const auto d_it = disk.characters.find(key);
        const auto m_it = memory.characters.find(key);
        if (d_it == disk.characters.end()) {
            out.characters.emplace(key, m_it->second);
        }
        else if (m_it == memory.characters.end()) {
            out.characters.emplace(key, d_it->second);
        }
        else {
            out.characters.emplace(key, MergeCharacter(d_it->second, m_it->second, result.diagnostics, conflict));
        }
    }

    if (conflict) {
        result.status = StoreOpStatus::MergeConflict;
        result.merged = std::move(out);
        return result;
    }
    CanonicalizeAccountStore(out);
    result.merged = std::move(out);
    result.status = StoreOpStatus::Ok;
    return result;
}

LoadStoreResult LoadAccountStore(
    const std::filesystem::path& quest_progress_dir,
    std::string_view account_key)
{
    const auto normalized = NormalizeAccountKey(account_key);
    if (normalized.empty()) {
        LoadStoreResult result;
        result.status = StoreOpStatus::ValidationError;
        AddDiag(result.diagnostics, "invalid account key");
        return result;
    }
    return LoadFromPaths(BuildAccountStorePaths(quest_progress_dir, normalized), normalized);
}

SaveStoreResult SaveMergedAccountStore(
    const std::filesystem::path& quest_progress_dir,
    std::string_view account_key,
    const AccountProgressStore& memory_store,
    unsigned long lock_timeout_ms)
{
    SaveStoreResult result;
    const auto normalized = NormalizeAccountKey(account_key);
    if (normalized.empty() || (!memory_store.account_key.empty() && NormalizeAccountKey(memory_store.account_key) != normalized)) {
        result.status = StoreOpStatus::ValidationError;
        AddDiag(result.diagnostics, "invalid or mismatched account key");
        result.diagnostics.dirty_retained = true;
        return result;
    }

    auto memory = memory_store;
    memory.account_key = normalized;

    const auto paths = BuildAccountStorePaths(quest_progress_dir, normalized);
    AccountMutex mutex(BuildAccountMutexName(normalized));
    const auto wait_kind = mutex.Acquire(lock_timeout_ms);
    if (wait_kind == WaitAcquireKind::Timeout) {
        result.status = StoreOpStatus::LockTimeout;
        AddDiag(result.diagnostics, "account mutex timeout; dirty retained");
        result.diagnostics.dirty_retained = true;
        return result;
    }
    if (wait_kind == WaitAcquireKind::Failed) {
        result.status = StoreOpStatus::LockFailed;
        result.diagnostics.win_error = mutex.create_error();
        AddDiag(result.diagnostics, "account mutex wait failed; dirty retained");
        result.diagnostics.dirty_retained = true;
        return result;
    }
    if (wait_kind == WaitAcquireKind::Abandoned) {
        result.diagnostics.abandoned_lock = true;
        AddDiag(result.diagnostics, "WAIT_ABANDONED: re-read and validate before merge");
    }

    auto loaded = LoadFromPaths(paths, normalized);
    if (loaded.status == StoreOpStatus::UnsupportedDiskMajor) {
        result.status = StoreOpStatus::UnsupportedDiskMajor;
        result.diagnostics = std::move(loaded.diagnostics);
        result.diagnostics.dirty_retained = true;
        AddDiag(result.diagnostics, "refusing overwrite of unsupported newer disk store");
        return result;
    }
    if (loaded.status == StoreOpStatus::CodecError) {
        result.status = StoreOpStatus::CodecError;
        result.diagnostics = std::move(loaded.diagnostics);
        result.diagnostics.dirty_retained = true;
        return result;
    }

    auto merged = MergeAccountStores(loaded.store, memory);
    if (merged.status != StoreOpStatus::Ok) {
        result.status = merged.status;
        result.diagnostics = std::move(merged.diagnostics);
        result.diagnostics.dirty_retained = true;
        return result;
    }

    auto serialized = SerializeAccountStoreJson(merged.merged);
    if (serialized.status != CodecStatus::Ok) {
        result.status = StoreOpStatus::CodecError;
        result.diagnostics.codec = serialized.diagnostics;
        result.diagnostics.dirty_retained = true;
        AddDiag(result.diagnostics, "serialize failed");
        return result;
    }

    const auto write = AtomicJson::WriteAtomicUtf8(paths.primary, paths.backup, serialized.utf8_json);
    if (!write.ok) {
        result.status = StoreOpStatus::IoError;
        result.diagnostics.win_error = write.win_error;
        result.diagnostics.dirty_retained = true;
        AddDiag(result.diagnostics, write.message);
        return result;
    }

    result.used_move_file_ex = write.used_move_file_ex;
    result.used_replace_file = write.used_replace_file;
    result.merged = std::move(merged.merged);
    result.status = StoreOpStatus::Ok;
    AddDiag(result.diagnostics, write.message);
    return result;
}

} // namespace QuestProgress
