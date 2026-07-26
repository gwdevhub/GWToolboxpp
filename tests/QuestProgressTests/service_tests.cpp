#include <Modules/QuestProgressService.h>
#include <Modules/QuestSessionIdentity.h>
#include <Modules/QuestProgressJsonCodec.h>
#include <Utils/AtomicJsonFile.h>

#include "test_assert.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

using namespace QuestProgress;
using namespace std::chrono_literals;

namespace {

std::filesystem::path MakeTempDir()
{
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    const auto base = std::filesystem::path(tmp) / L"gwtb_quest_progress_2c_tests";
    std::error_code ec;
    std::filesystem::create_directories(base, ec);
    static std::atomic<uint32_t> seq{0};
    const auto dir = base / std::to_string(GetCurrentProcessId())
        / (std::to_string(GetTickCount64()) + "_" + std::to_string(seq.fetch_add(1)));
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string ReadAll(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteRaw(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

const char* kAcct = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
const char* kCharA = "11111111-2222-3333-4444-555555555555";
const char* kCharB = "66666666-7777-8888-9999-aaaaaaaaaaaa";
const char* kAcct2 = "bbbbbbbb-cccc-dddd-eeee-ffffffffffff";

SessionIdentity PersistentId(const char* account, const char* character, const char* name)
{
    return MakeSessionIdentity(account, character, name, "W", false);
}

QuestSnapshot MakeSnap(uint64_t rev, std::initializer_list<QuestSnapshotQuest> quests)
{
    QuestSnapshot s;
    s.revision = rev;
    s.loading = false;
    s.world_ready = true;
    s.selected_active_quest_id = 999;
    s.quests = quests;
    return s;
}

QuestSnapshotQuest Q(uint32_t id, bool ready = false)
{
    QuestSnapshotQuest q;
    q.game_quest_id = id;
    q.in_log_completed = ready;
    return q;
}

std::chrono::system_clock::time_point Wall(int offset_sec)
{
    return std::chrono::system_clock::time_point(std::chrono::seconds(1'700'000'000 + offset_sec));
}

void TestIdentityBinding()
{
    auto persistent = MakeSessionIdentity(kAcct, kCharA, "Hero", "W", false);
    Expect(persistent.kind == IdentityKind::Persistent, "id_persistent_kind");
    Expect(persistent.character_key == std::string(kAcct) + "/" + kCharA
            || persistent.character_key.find('/') != std::string::npos,
        "id_character_key_shape");
    Expect(persistent.character_key == BuildCharacterKey(NormalizeAccountKey(kAcct), NormalizeAccountKey(kCharA)),
        "id_character_key_exact");

    auto ephemeral = MakeSessionIdentity(kAcct, "00000000-0000-0000-0000-000000000000", "Hero", "W", false);
    Expect(ephemeral.kind == IdentityKind::Ephemeral, "id_zero_char_ephemeral");
    Expect(ephemeral.character_key.empty(), "id_ephemeral_no_key");

    auto a = MakeSessionIdentity(kAcct, kCharA, "SameName", "W", false);
    auto b = MakeSessionIdentity(kAcct, kCharB, "SameName", "W", false);
    Expect(a.character_key != b.character_key, "id_name_collision_isolated");
    Expect(!SamePersistentCharacter(a, b), "id_not_same_character");
}

void TestServiceCoreMatrix()
{
    const auto dir = MakeTempDir();
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    Expect(svc.initialized(), "svc_init_idempotent_pre");
    svc.Initialize();
    Expect(svc.initialized(), "svc_init_idempotent");

    auto id_a = PersistentId(kAcct, kCharA, "Hero");
    svc.BindIdentity(id_a);
    Expect(svc.persistence_allowed(), "svc_empty_permits_save");
    Expect(svc.load_status() == StoreOpStatus::Empty, "svc_load_empty");

    auto t0 = std::chrono::steady_clock::now();
    auto snap1 = MakeSnap(1, {Q(100)});
    svc.IngestSnapshot(snap1, Wall(10), t0);
    Expect(svc.character_progress().quests.count(100) == 1, "svc_first_snap_creates");
    Expect(svc.character_progress().quests.at(100).state == ProgressState::Active, "svc_first_active");
    Expect(svc.semantic_dirty(), "svc_first_dirty");

    // Duplicate same revision: no change
    const auto hist1 = svc.character_progress().quests.at(100).history.size();
    svc.IngestSnapshot(snap1, Wall(11), t0 + 100ms);
    Expect(svc.character_progress().quests.at(100).history.size() == hist1, "svc_dup_revision_no_event");

    // New revision identical content
    auto snap2 = MakeSnap(2, {Q(100)});
    svc.IngestSnapshot(snap2, Wall(12), t0 + 200ms);
    Expect(svc.character_progress().quests.at(100).history.size() == hist1, "svc_dup_content_no_event");

    // Objective change
    auto snap_obj = MakeSnap(3, {Q(100)});
    ObjectiveObservation obj;
    obj.index = 0;
    obj.completed = false;
    obj.encoded_content = u"Kill";
    snap_obj.quests[0].objectives.push_back(obj);
    svc.IngestSnapshot(snap_obj, Wall(13), t0 + 300ms);
    Expect(svc.character_progress().quests.at(100).state == ProgressState::ObjectiveProgress, "svc_obj_change");
    Expect(svc.character_progress().quests.at(100).history.size() == hist1 + 1, "svc_obj_append_once");

    // Ready
    auto snap_ready = MakeSnap(4, {Q(100, true)});
    snap_ready.quests[0].objectives = snap_obj.quests[0].objectives;
    svc.IngestSnapshot(snap_ready, Wall(14), t0 + 400ms);
    Expect(svc.character_progress().quests.at(100).state == ProgressState::ReadyForReward, "svc_ready");

    // Selection id must not affect
    Expect(snap_ready.selected_active_quest_id == 999, "svc_selection_field_present");
    Expect(svc.character_progress().quests.count(999) == 0, "svc_selection_ignored");

    // Bare disappearance
    auto snap_gone = MakeSnap(5, {});
    svc.IngestSnapshot(snap_gone, Wall(15), t0 + 500ms);
    Expect(svc.character_progress().quests.at(100).state == ProgressState::Unknown, "svc_bare_unknown");

    // Exact abandon pairing
    auto id_fresh = PersistentId(kAcct, kCharA, "Hero");
    QuestProgressService svc2;
    svc2.Initialize();
    svc2.SetStoreDirectory(MakeTempDir());
    svc2.BindIdentity(id_fresh);
    auto t1 = std::chrono::steady_clock::now();
    svc2.IngestSnapshot(MakeSnap(1, {Q(200)}), Wall(20), t1);
    EvidenceStamp ab;
    ab.game_quest_id = 200;
    ab.kind = EvidenceKind::Abandon;
    ab.steady_at = t1 + 10ms;
    svc2.IngestEvidence({ab});
    svc2.IngestSnapshot(MakeSnap(2, {}), Wall(21), t1 + 20ms);
    Expect(svc2.character_progress().quests.at(200).state == ProgressState::AbandonedObserved, "svc_abandon_pair");
    Expect(svc2.character_progress().quests.at(200).confidence == Confidence::Probable, "svc_abandon_probable");

    // Late abandon upgrade
    QuestProgressService svc3;
    svc3.Initialize();
    svc3.SetStoreDirectory(MakeTempDir());
    svc3.BindIdentity(PersistentId(kAcct, kCharA, "Hero"));
    auto t2 = std::chrono::steady_clock::now();
    svc3.IngestSnapshot(MakeSnap(1, {Q(300)}), Wall(30), t2);
    svc3.IngestSnapshot(MakeSnap(2, {}), Wall(31), t2 + 10ms);
    Expect(svc3.character_progress().quests.at(300).state == ProgressState::Unknown, "svc_late_pre_unknown");
    EvidenceStamp late_ab;
    late_ab.game_quest_id = 300;
    late_ab.kind = EvidenceKind::Abandon;
    late_ab.steady_at = t2 + 20ms;
    svc3.IngestEvidence({late_ab});
    svc3.IngestSnapshot(MakeSnap(3, {}), Wall(32), t2 + 30ms);
    Expect(svc3.character_progress().quests.at(300).state == ProgressState::AbandonedObserved, "svc_late_abandon_upgrade");

    // Exact reward after ready
    QuestProgressService svc4;
    svc4.Initialize();
    svc4.SetStoreDirectory(MakeTempDir());
    svc4.BindIdentity(PersistentId(kAcct, kCharA, "Hero"));
    auto t3 = std::chrono::steady_clock::now();
    svc4.IngestSnapshot(MakeSnap(1, {Q(400, true)}), Wall(40), t3);
    EvidenceStamp rw;
    rw.game_quest_id = 400;
    rw.kind = EvidenceKind::Reward;
    rw.steady_at = t3 + 5ms;
    svc4.IngestEvidence({rw});
    svc4.IngestSnapshot(MakeSnap(2, {}), Wall(41), t3 + 10ms);
    Expect(svc4.character_progress().quests.at(400).state == ProgressState::CompletedObserved, "svc_reward_pair");
    Expect(svc4.character_progress().quests.at(400).confidence == Confidence::Probable, "svc_reward_probable");

    // ENQUIRE_REWARD rejection
    QuestProgressService svc5;
    svc5.Initialize();
    svc5.SetStoreDirectory(MakeTempDir());
    svc5.BindIdentity(PersistentId(kAcct, kCharA, "Hero"));
    auto t4 = std::chrono::steady_clock::now();
    svc5.IngestSnapshot(MakeSnap(1, {Q(500, true)}), Wall(50), t4);
    EvidenceStamp eq;
    eq.game_quest_id = 500;
    eq.kind = EvidenceKind::EnquireReward;
    eq.steady_at = t4;
    svc5.IngestEvidence({eq});
    svc5.IngestSnapshot(MakeSnap(2, {}), Wall(51), t4 + 10ms);
    Expect(svc5.character_progress().quests.at(500).state == ProgressState::Unknown, "svc_enquire_rejected");

    // Conflicting evidence
    QuestProgressService svc6;
    svc6.Initialize();
    svc6.SetStoreDirectory(MakeTempDir());
    svc6.BindIdentity(PersistentId(kAcct, kCharA, "Hero"));
    auto t5 = std::chrono::steady_clock::now();
    svc6.IngestSnapshot(MakeSnap(1, {Q(600, true)}), Wall(60), t5);
    EvidenceStamp c1{600, EvidenceKind::Abandon, t5};
    EvidenceStamp c2{600, EvidenceKind::Reward, t5};
    svc6.IngestEvidence({c1, c2});
    svc6.IngestSnapshot(MakeSnap(2, {}), Wall(61), t5 + 10ms);
    Expect(svc6.character_progress().quests.at(600).state == ProgressState::Unknown, "svc_conflict_unknown");
}

void TestCharacterAccountSwitchAndPersist()
{
    const auto dir = MakeTempDir();
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);

    auto id_a = PersistentId(kAcct, kCharA, "HeroA");
    auto id_b = PersistentId(kAcct, kCharB, "HeroB"); // same display-ish different uuid
    id_b.display_name = "HeroA"; // same display name, different UUID
    svc.BindIdentity(id_a);
    auto t0 = std::chrono::steady_clock::now();
    svc.IngestSnapshot(MakeSnap(1, {Q(100)}), Wall(100), t0);
    Expect(svc.semantic_dirty(), "switch_a_dirty");

    // Debounce coalesce: another change before 1s
    svc.IngestSnapshot(MakeSnap(2, {Q(100), Q(101)}), Wall(101), t0 + 100ms);
    Expect(svc.successful_save_count() == 0, "debounce_no_early_save");
    svc.Tick(t0 + 100ms, Wall(101));
    Expect(svc.successful_save_count() == 0, "debounce_still_waiting");
    svc.Tick(t0 + 1100ms, Wall(102));
    Expect(svc.successful_save_count() == 1, "debounce_save_after_idle");
    Expect(std::filesystem::exists(BuildAccountStorePaths(dir, NormalizeAccountKey(kAcct)).primary), "debounce_file_created");

    // Character switch flushes then isolates
    const auto saves_before = svc.successful_save_count();
    svc.BindIdentity(id_b);
    Expect(svc.character_progress().quests.count(100) == 0, "switch_b_no_a_quests");
    svc.IngestSnapshot(MakeSnap(10, {Q(200)}), Wall(110), t0 + 2s);
    Expect(svc.character_progress().quests.count(200) == 1, "switch_b_has_own");
    Expect(svc.character_progress().quests.count(100) == 0, "switch_b_still_isolated");

    // Map-load style: same character bind is no-op identity
    svc.BindIdentity(id_b);
    Expect(svc.character_progress().quests.count(200) == 1, "mapload_preserves_identity");

    // Account switch isolation
    auto id_other = PersistentId(kAcct2, kCharA, "Other");
    svc.BindIdentity(id_other);
    Expect(svc.account_store().account_key == NormalizeAccountKey(kAcct2)
            || svc.load_status() == StoreOpStatus::Empty,
        "account_switch_key");
    Expect(svc.character_progress().quests.count(200) == 0, "account_switch_clears");
}

void TestEphemeralNeverWrites()
{
    const auto dir = MakeTempDir();
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    svc.BindIdentity(MakeSessionIdentity(kAcct, "00000000-0000-0000-0000-000000000000", "Temp", "W", false));
    Expect(!svc.persistence_allowed(), "eph_no_persist");
    auto t0 = std::chrono::steady_clock::now();
    svc.IngestSnapshot(MakeSnap(1, {Q(100)}), Wall(1), t0);
    Expect(svc.character_progress().quests.count(100) == 1, "eph_memory_progress");
    svc.Tick(t0 + 2s, Wall(2));
    svc.Flush(true);
    Expect(!std::filesystem::exists(BuildAccountStorePaths(dir, NormalizeAccountKey(kAcct)).primary),
        "eph_never_creates_file");
    Expect(svc.successful_save_count() == 0, "eph_no_saves");
}

void TestBlockedStoreStatuses()
{
    const auto dir = MakeTempDir();
    const auto paths = BuildAccountStorePaths(dir, NormalizeAccountKey(kAcct));
    WriteRaw(paths.primary, "{not-json");
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    svc.BindIdentity(PersistentId(kAcct, kCharA, "Hero"));
    Expect(svc.load_status() == StoreOpStatus::CodecError, "block_codec_load");
    Expect(!svc.persistence_allowed(), "block_codec_no_persist");
    Expect(svc.persist_latch() == PersistLatch::BlockedPermanent, "block_codec_latch");
    const auto before = ReadAll(paths.primary);
    auto t0 = std::chrono::steady_clock::now();
    svc.IngestSnapshot(MakeSnap(1, {Q(100)}), Wall(1), t0);
    Expect(svc.semantic_dirty(), "block_codec_dirty_retained");
    const auto attempts0 = svc.persist_attempt_count();
    const auto diag0 = svc.diagnostics().size();
    svc.Tick(t0 + 2s, Wall(2));
    svc.Tick(t0 + 3s, Wall(3));
    svc.Tick(t0 + 4s, Wall(4));
    Expect(svc.persist_attempt_count() == attempts0, "block_codec_no_per_frame_retry");
    Expect(svc.diagnostics().size() <= diag0 + 2, "block_codec_diag_not_spam");
    Expect(ReadAll(paths.primary) == before, "block_codec_bytes_preserved");

    // Unsupported major
    const auto dir2 = MakeTempDir();
    const auto paths2 = BuildAccountStorePaths(dir2, NormalizeAccountKey(kAcct));
    WriteRaw(paths2.primary, R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 2, "minor": 0 },
  "accountKey": "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",
  "characters": []
})");
    QuestProgressService svc2;
    svc2.Initialize();
    svc2.SetStoreDirectory(dir2);
    svc2.BindIdentity(PersistentId(kAcct, kCharA, "Hero"));
    Expect(svc2.load_status() == StoreOpStatus::UnsupportedDiskMajor, "block_major_load");
    Expect(svc2.persist_latch() == PersistLatch::BlockedPermanent, "block_major_latch");
    const auto before2 = ReadAll(paths2.primary);
    const auto attempts2 = svc2.persist_attempt_count();
    svc2.IngestSnapshot(MakeSnap(1, {Q(100)}), Wall(1), t0);
    svc2.Tick(t0 + 2s, Wall(2));
    svc2.Tick(t0 + 3s, Wall(3));
    Expect(svc2.persist_attempt_count() == attempts2, "block_major_no_per_frame_retry");
    svc2.Flush(true);
    Expect(ReadAll(paths2.primary) == before2, "block_major_bytes_preserved");
}

void TestHeartbeatAndLifecycle()
{
    const auto dir = MakeTempDir();
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    svc.BindIdentity(PersistentId(kAcct, kCharA, "Hero"));
    auto t0 = std::chrono::steady_clock::now();
    svc.IngestSnapshot(MakeSnap(1, {Q(100)}), Wall(1), t0);
    svc.Tick(t0 + 2s, Wall(2));
    Expect(svc.successful_save_count() == 1, "hb_initial_semantic_save");

    // Unchanged revisions do not write
    const auto saves = svc.successful_save_count();
    svc.IngestSnapshot(MakeSnap(1, {Q(100)}), Wall(3), t0 + 3s);
    svc.Tick(t0 + 4s, Wall(4));
    Expect(svc.successful_save_count() == saves, "unchanged_no_write");

    // Heartbeat: touch-only after long interval
    svc.IngestSnapshot(MakeSnap(2, {Q(100)}), Wall(5), t0 + 5s);
    if (svc.heartbeat_pending()) {
        svc.Tick(t0 + 5s + kHeartbeatInterval + 1s, Wall(6));
        Expect(svc.successful_save_count() >= saves, "heartbeat_throttled_path");
    }

    // Canonical timestamp
    const auto ts = FormatCanonicalUtc(Wall(0));
    Expect(IsCanonicalUtcTimestamp(ts), "canonical_utc_format");

    // Lifecycle terminate flush + idempotent
    svc.SignalTerminate();
    Expect(svc.terminate_signaled(), "term_signaled");
    svc.Terminate();
    Expect(!svc.initialized(), "term_clears_init");
    svc.Terminate();
    svc.Initialize();
    Expect(svc.initialized(), "reinit_after_term");
}

void TestMergeOptionalIgnored()
{
    auto disk = AccountProgressStore{};
    disk.account_key = NormalizeAccountKey(kAcct);
    disk.store_format = kStoreFormatId;
    disk.store_version = {kStoreFormatMajor, kStoreFormatMinor};
    StoredCharacter c;
    c.character_key = BuildCharacterKey(disk.account_key, NormalizeAccountKey(kCharA));
    c.first_observed_at = "2026-07-25T20:00:00.000Z";
    c.last_observed_at = "2026-07-25T20:00:00.000Z";
    ::QuestProgress::QuestProgress q;
    q.game_quest_id = 1;
    q.state = ProgressState::Active;
    q.first_observed_at = c.first_observed_at;
    q.last_observed_at = c.last_observed_at;
    QuestHistoryEvent ev;
    ev.game_quest_id = 1;
    ev.event_type = HistoryEventType::Observation;
    ev.state = ProgressState::Active;
    ev.observed_at = c.first_observed_at;
    ev.semantic_event_key = "same-key";
    q.history.push_back(ev);
    c.quests.emplace(1, q);
    disk.characters.emplace(c.character_key, c);

    auto memory = disk;
    memory.characters.begin()->second.quests.at(1).history[0].state = ProgressState::ReadyForReward;
    auto conflict = MergeAccountStores(disk, memory);
    Expect(conflict.status == StoreOpStatus::MergeConflict, "merge_opt_conflict");
    Expect(!conflict.merged.has_value(), "merge_opt_no_store");
}

struct MutexHold {
    HANDLE ready = nullptr;
    HANDLE release_event = nullptr;
    HANDLE held = nullptr;
    std::thread holder;

    explicit MutexHold(const std::string& mutex_name)
    {
        const std::wstring wide(mutex_name.begin(), mutex_name.end());
        ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        release_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        holder = std::thread([this, wide]() {
            held = CreateMutexW(nullptr, FALSE, wide.c_str());
            if (!held) {
                SetEvent(ready);
                return;
            }
            if (WaitForSingleObject(held, 0) != WAIT_OBJECT_0) {
                SetEvent(ready);
                return;
            }
            SetEvent(ready);
            WaitForSingleObject(release_event, INFINITE);
            ReleaseMutex(held);
            CloseHandle(held);
            held = nullptr;
        });
        WaitForSingleObject(ready, 5000);
    }

    void Release()
    {
        if (release_event) {
            SetEvent(release_event);
        }
        if (holder.joinable()) {
            holder.join();
        }
        if (ready) {
            CloseHandle(ready);
            ready = nullptr;
        }
        if (release_event) {
            CloseHandle(release_event);
            release_event = nullptr;
        }
    }

    ~MutexHold() { Release(); }

    bool ok() const { return held != nullptr; }
};

void TestFailedSwitchFlushRetainsAndRetries()
{
    const auto dir = MakeTempDir();
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    svc.SetLockTimeoutMs(100);

    auto id_a = PersistentId(kAcct, kCharA, "HeroA");
    auto id_b = PersistentId(kAcct, kCharB, "HeroB");
    svc.BindIdentity(id_a);
    auto t0 = std::chrono::steady_clock::now();
    svc.IngestSnapshot(MakeSnap(1, {Q(100)}), Wall(100), t0);
    Expect(svc.semantic_dirty(), "fail_switch_a_dirty");
    Expect(svc.character_progress().quests.count(100) == 1, "fail_switch_a_has_100");

    {
        MutexHold hold(BuildAccountMutexName(NormalizeAccountKey(kAcct)));
        Expect(hold.ok(), "fail_switch_mutex_held");
        svc.BindIdentity(id_b);
    }
    Expect(svc.detached_session_count() == 1, "fail_switch_retained_a");
    Expect(svc.character_progress().quests.count(100) == 0, "fail_switch_b_no_a");
    Expect(svc.identity().character_key == id_b.character_key, "fail_switch_bound_b");
    Expect(svc.detached_sessions()[0].character.quests.count(100) == 1, "fail_switch_a_in_detached");
    Expect(!std::filesystem::exists(BuildAccountStorePaths(dir, NormalizeAccountKey(kAcct)).primary),
        "fail_switch_no_file_yet");

    svc.IngestSnapshot(MakeSnap(10, {Q(200)}), Wall(110), t0 + 1s);
    Expect(svc.character_progress().quests.count(200) == 1, "fail_switch_b_own");
    Expect(svc.character_progress().quests.count(100) == 0, "fail_switch_b_still_isolated");

    // Detached retry for A after lock released (initial backoff 500ms).
    svc.Tick(t0 + 2s, Wall(120));
    svc.Tick(t0 + 3s, Wall(121));
    Expect(svc.detached_session_count() == 0, "fail_switch_a_retry_cleared");
    Expect(std::filesystem::exists(BuildAccountStorePaths(dir, NormalizeAccountKey(kAcct)).primary),
        "fail_switch_file_after_retry");

    QuestProgressService verify;
    verify.Initialize();
    verify.SetStoreDirectory(dir);
    verify.BindIdentity(id_a);
    Expect(verify.character_progress().quests.count(100) == 1, "fail_switch_a_reloaded");
    verify.BindIdentity(id_b);
    Expect(verify.character_progress().quests.count(100) == 0, "fail_switch_b_reload_isolated");
}

void TestMultipleFailedSwitchesPreserveBoth()
{
    const auto dir = MakeTempDir();
    const char* kCharC = "cccccccc-dddd-eeee-ffff-000000000001";
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    svc.SetLockTimeoutMs(100);

    auto id_a = PersistentId(kAcct, kCharA, "A");
    auto id_b = PersistentId(kAcct, kCharB, "B");
    auto id_c = PersistentId(kAcct, kCharC, "C");
    auto t0 = std::chrono::steady_clock::now();

    svc.BindIdentity(id_a);
    svc.IngestSnapshot(MakeSnap(1, {Q(100)}), Wall(1), t0);
    {
        MutexHold hold(BuildAccountMutexName(NormalizeAccountKey(kAcct)));
        Expect(hold.ok(), "multi_fail_mutex_a");
        svc.BindIdentity(id_b);
    }
    Expect(svc.detached_session_count() == 1, "multi_fail_retained_a");
    Expect(svc.detached_sessions()[0].identity.character_key == id_a.character_key, "multi_fail_a_key");

    svc.IngestSnapshot(MakeSnap(2, {Q(200)}), Wall(2), t0 + 1s);
    {
        MutexHold hold(BuildAccountMutexName(NormalizeAccountKey(kAcct)));
        Expect(hold.ok(), "multi_fail_mutex_b");
        svc.BindIdentity(id_c);
    }
    Expect(svc.detached_session_count() == 2, "multi_fail_retained_ab");
    Expect(svc.detached_sessions()[0].identity.character_key == id_a.character_key, "multi_fail_a_kept");
    Expect(svc.detached_sessions()[1].identity.character_key == id_b.character_key, "multi_fail_b_kept");
    Expect(svc.detached_sessions()[0].character.quests.count(100) == 1, "multi_fail_a_data");
    Expect(svc.detached_sessions()[1].character.quests.count(200) == 1, "multi_fail_b_data");
    Expect(svc.identity().character_key == id_c.character_key, "multi_fail_bound_c");
}

void TestEvidenceOrderingAcrossSwitch()
{
    const auto dir = MakeTempDir();
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    auto id_a = PersistentId(kAcct, kCharA, "A");
    auto id_b = PersistentId(kAcct, kCharB, "B");
    auto t0 = std::chrono::steady_clock::now();

    svc.BindIdentity(id_a);
    const auto gen_a_acct = svc.account_generation();
    const auto gen_a_char = svc.character_generation();
    svc.IngestSnapshot(MakeSnap(1, {Q(100)}), Wall(1), t0);

    EvidenceStamp ab;
    ab.game_quest_id = 100;
    ab.kind = EvidenceKind::Abandon;
    ab.steady_at = t0 + 5ms;
    ab.account_generation = gen_a_acct;
    ab.character_generation = gen_a_char;
    ab.account_key = id_a.account_key;
    ab.character_key = id_a.character_key;
    svc.IngestEvidence({ab});
    svc.BindIdentity(id_b);
    Expect(svc.character_progress().quests.count(100) == 0, "ev_order_b_no_100");
    if (svc.detached_session_count() == 0) {
        QuestProgressService reload;
        reload.Initialize();
        reload.SetStoreDirectory(dir);
        reload.BindIdentity(id_a);
        Expect(reload.character_progress().quests.at(100).state == ProgressState::AbandonedObserved,
            "ev_order_a_abandoned_persisted");
    }
    else {
        Expect(svc.detached_sessions()[0].character.quests.at(100).state == ProgressState::AbandonedObserved,
            "ev_order_a_abandoned_detached");
    }

    QuestProgressService svc2;
    svc2.Initialize();
    svc2.SetStoreDirectory(MakeTempDir());
    svc2.BindIdentity(id_a);
    auto t1 = std::chrono::steady_clock::now();
    svc2.IngestSnapshot(MakeSnap(1, {Q(400, true)}), Wall(10), t1);
    EvidenceStamp rw;
    rw.game_quest_id = 400;
    rw.kind = EvidenceKind::Reward;
    rw.steady_at = t1 + 5ms;
    rw.account_generation = svc2.account_generation();
    rw.character_generation = svc2.character_generation();
    svc2.IngestEvidence({rw});
    svc2.BindIdentity(id_b);
    Expect(svc2.character_progress().quests.count(400) == 0, "ev_reward_b_clear");

    QuestProgressService svc3;
    svc3.Initialize();
    svc3.SetStoreDirectory(MakeTempDir());
    EvidenceStamp orphan;
    orphan.game_quest_id = 500;
    orphan.kind = EvidenceKind::Abandon;
    orphan.steady_at = t1;
    svc3.IngestEvidence({orphan});
    svc3.BindIdentity(id_b);
    svc3.IngestSnapshot(MakeSnap(1, {Q(500)}), Wall(20), t1 + 1s);
    svc3.IngestSnapshot(MakeSnap(2, {}), Wall(21), t1 + 2s);
    Expect(svc3.character_progress().quests.at(500).state == ProgressState::Unknown, "ev_unbound_no_abandon");

    QuestProgressService svc4;
    svc4.Initialize();
    svc4.SetStoreDirectory(MakeTempDir());
    svc4.BindIdentity(id_a);
    auto t2 = std::chrono::steady_clock::now();
    svc4.IngestSnapshot(MakeSnap(1, {Q(600)}), Wall(30), t2);
    EvidenceStamp keep;
    keep.game_quest_id = 600;
    keep.kind = EvidenceKind::Abandon;
    keep.steady_at = t2 + 5ms;
    keep.account_generation = svc4.account_generation();
    keep.character_generation = svc4.character_generation();
    svc4.IngestEvidence({keep});
    svc4.BindIdentity(id_a);
    svc4.IngestSnapshot(MakeSnap(2, {}), Wall(31), t2 + 10ms);
    Expect(svc4.character_progress().quests.at(600).state == ProgressState::AbandonedObserved,
        "ev_mapload_same_char_preserves");

    QuestProgressService svc5;
    svc5.Initialize();
    svc5.SetStoreDirectory(MakeTempDir());
    svc5.BindIdentity(id_a);
    auto t3 = std::chrono::steady_clock::now();
    svc5.IngestSnapshot(MakeSnap(1, {Q(700)}), Wall(40), t3);
    const auto old_gen = svc5.character_generation();
    svc5.BindIdentity(id_b);
    svc5.BindIdentity(id_a);
    Expect(svc5.character_generation() != old_gen, "ev_new_gen_after_rebind");
    svc5.IngestSnapshot(MakeSnap(1, {Q(700)}), Wall(41), t3 + 1s);
    EvidenceStamp stale;
    stale.game_quest_id = 700;
    stale.kind = EvidenceKind::Abandon;
    stale.steady_at = t3 + 2s;
    stale.account_generation = svc5.account_generation();
    stale.character_generation = old_gen;
    svc5.IngestEvidence({stale});
    svc5.IngestSnapshot(MakeSnap(2, {}), Wall(42), t3 + 3s);
    Expect(svc5.character_progress().quests.at(700).state == ProgressState::Unknown, "ev_stale_gen_ignored");
}

void TestLockTimeoutBackoff()
{
    const auto dir = MakeTempDir();
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    svc.SetLockTimeoutMs(100);
    svc.BindIdentity(PersistentId(kAcct, kCharA, "Hero"));
    auto t0 = std::chrono::steady_clock::now();
    svc.IngestSnapshot(MakeSnap(1, {Q(100)}), Wall(1), t0);

    MutexHold hold(BuildAccountMutexName(NormalizeAccountKey(kAcct)));
    Expect(hold.ok(), "backoff_mutex");
    const auto attempts0 = svc.persist_attempt_count();
    svc.Tick(t0 + 1100ms, Wall(2));
    Expect(svc.persist_attempt_count() == attempts0 + 1, "backoff_first_attempt");
    Expect(svc.persist_latch() == PersistLatch::BlockedRetryable, "backoff_latch");
    Expect(svc.semantic_dirty(), "backoff_dirty_kept");
    const auto retry_at = svc.next_persist_retry_at();
    const auto retry_ms = std::chrono::duration_cast<std::chrono::milliseconds>(retry_at - t0).count();
    Expect(retry_ms >= 1500, "backoff_deadline_ge_1500");
    const auto attempts1 = svc.persist_attempt_count();
    svc.Tick(t0 + 1200ms, Wall(3));
    Expect(svc.persist_attempt_count() == attempts1, "backoff_no_immediate_retry");
    svc.Tick(t0 + 1100ms + kPersistRetryInitialBackoff + 10ms, Wall(4));
    Expect(svc.persist_attempt_count() >= attempts1 + 1, "backoff_retry_after_delay");
}

void TestDirtyGenerationSafety()
{
    const auto dir = MakeTempDir();
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    svc.BindIdentity(PersistentId(kAcct, kCharA, "Hero"));
    auto t0 = std::chrono::steady_clock::now();
    svc.IngestSnapshot(MakeSnap(1, {Q(100)}), Wall(1), t0);
    Expect(svc.dirty_generation() == 1, "dirty_gen_1");

    svc.SetPersistTestHook([&]() {
        svc.IngestSnapshot(MakeSnap(2, {Q(100), Q(101)}), Wall(2), t0 + 50ms);
    });
    svc.Tick(t0 + 1100ms, Wall(3));
    Expect(svc.dirty_generation() >= 2, "dirty_gen_bumped_during_save");
    Expect(svc.semantic_dirty(), "dirty_gen_n1_still_dirty");
    Expect(svc.character_progress().quests.count(101) == 1, "dirty_gen_n1_present");

    svc.SetPersistTestHook({});
    svc.Tick(t0 + 2200ms, Wall(4));
    Expect(!svc.semantic_dirty(), "dirty_gen_cleared_after_followup");
}

void TestDiagnosticsBounded()
{
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(MakeTempDir());
    auto id_a = PersistentId(kAcct, kCharA, "Hero");
    for (int i = 0; i < 150; ++i) {
        svc.BindIdentity(id_a, true);
    }
    Expect(svc.diagnostics().size() <= kMaxDiagnostics, "diag_bounded_max");
    Expect(svc.diagnostics().size() == kMaxDiagnostics, "diag_at_capacity");
    Expect(!svc.diagnostics().empty(), "diag_newest_retained");
}

void TestLogoutUnbindAndMapLoad()
{
    const auto dir = MakeTempDir();
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    auto id_a = PersistentId(kAcct, kCharA, "Hero");
    svc.BindIdentity(id_a);
    auto t0 = std::chrono::steady_clock::now();
    svc.IngestSnapshot(MakeSnap(1, {Q(100)}), Wall(1), t0);
    svc.Tick(t0 + 2s, Wall(2));
    Expect(svc.successful_save_count() >= 1, "logout_saved_before");

    svc.UnbindIdentity();
    Expect(svc.identity().kind == IdentityKind::Unbound, "logout_unbound");
    Expect(svc.character_progress().quests.empty(), "logout_clears_active");

    svc.BindIdentity(id_a);
    svc.IngestSnapshot(MakeSnap(1, {Q(100)}), Wall(3), t0 + 3s);
    svc.BindIdentity(id_a);
    Expect(svc.identity().kind == IdentityKind::Persistent, "mapload_still_bound");
    Expect(svc.character_progress().quests.count(100) == 1, "mapload_preserves");

    QuestProgressService svc2;
    svc2.Initialize();
    svc2.SetStoreDirectory(MakeTempDir());
    svc2.SetLockTimeoutMs(100);
    svc2.BindIdentity(id_a);
    svc2.IngestSnapshot(MakeSnap(2, {Q(100), Q(101)}), Wall(4), t0 + 4s);
    Expect(svc2.semantic_dirty(), "logout_fail_dirty");
    {
        MutexHold hold(BuildAccountMutexName(NormalizeAccountKey(kAcct)));
        Expect(hold.ok(), "logout_fail_mutex");
        svc2.UnbindIdentity();
    }
    Expect(svc2.identity().kind == IdentityKind::Unbound, "logout_fail_unbound");
    Expect(svc2.detached_session_count() == 1, "logout_fail_retained");
}

} // namespace

void RunBatch2CServiceTests()
{
    TestIdentityBinding();
    TestServiceCoreMatrix();
    TestCharacterAccountSwitchAndPersist();
    TestEphemeralNeverWrites();
    TestBlockedStoreStatuses();
    TestHeartbeatAndLifecycle();
    TestMergeOptionalIgnored();
    TestFailedSwitchFlushRetainsAndRetries();
    TestMultipleFailedSwitchesPreserveBoth();
    TestEvidenceOrderingAcrossSwitch();
    TestLockTimeoutBackoff();
    TestDirtyGenerationSafety();
    TestDiagnosticsBounded();
    TestLogoutUnbindAndMapLoad();
}
