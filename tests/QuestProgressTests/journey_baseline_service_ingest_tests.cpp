#include <Modules/QuestCharacterJourney.h>
#include <Modules/QuestProgressJsonCodec.h>
#include <Modules/QuestProgressService.h>
#include <Modules/QuestProgressStore.h>
#include <Modules/QuestSessionIdentity.h>

#include "test_assert.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

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

const char* kAcct = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
const char* kCharA = "11111111-2222-3333-4444-555555555555";
const char* kCharB = "66666666-7777-8888-9999-aaaaaaaaaaaa";
const char* kAcct2 = "bbbbbbbb-cccc-dddd-eeee-ffffffffffff";
constexpr const char* kTs = "2026-09-13T12:00:00.000Z";
constexpr const char* kTs2 = "2026-09-13T12:01:00.000Z";
constexpr const char* kTs3 = "2026-09-13T12:02:00.000Z";
constexpr const char* kTs4 = "2026-09-13T12:03:00.000Z";
constexpr const char* kBadTs = "2026-09-13T12:00:00";

std::filesystem::path MakeTempDir()
{
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    const auto base = std::filesystem::path(tmp) / L"gwtb_quest_baseline_ingest_tests";
    std::error_code ec;
    std::filesystem::create_directories(base, ec);
    static std::atomic<uint32_t> seq{0};
    const auto dir = base / std::to_string(GetCurrentProcessId())
        / (std::to_string(GetTickCount64()) + "_" + std::to_string(seq.fetch_add(1)));
    std::filesystem::create_directories(dir, ec);
    return dir;
}

SessionIdentity PersistentId(const char* account, const char* character, const char* name)
{
    return MakeSessionIdentity(account, character, name, "W", false);
}

std::chrono::system_clock::time_point Wall(int offset_sec)
{
    return std::chrono::system_clock::time_point(std::chrono::seconds(1'700'000'000 + offset_sec));
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

JourneySnapshotResult Stamp(JourneySnapshotResult snapshot, const SessionIdentity& id)
{
    snapshot.identity_captured = true;
    snapshot.account_key = id.account_key;
    snapshot.character_key = id.character_key;
    return snapshot;
}

JourneySnapshotResult MapsSample(
    const SessionIdentity& id,
    std::vector<uint32_t> ids,
    std::string_view observed_at = kTs)
{
    JourneySnapshotResult snapshot;
    snapshot.raw_flood.observed_at = std::string(observed_at);
    snapshot.raw_flood.maps = MakeRawIdSetFamilyObservation(true, true, std::move(ids));
    NormalizeRawJourneyFloodObservation(snapshot.raw_flood);
    return Stamp(std::move(snapshot), id);
}

void IngestMapsN(
    QuestProgressService& svc,
    const SessionIdentity& id,
    const std::vector<uint32_t>& ids,
    int n,
    std::string_view observed_at = kTs)
{
    for (int i = 0; i < n; ++i) {
        svc.IngestJourneySnapshot(MapsSample(id, ids, observed_at));
    }
}

const StoredCharacter& ActiveCharacter(const QuestProgressService& svc, const SessionIdentity& id)
{
    return svc.account_store().characters.at(id.character_key);
}

void TestVeteranBootstrapZeroEventsPersistReload()
{
    const auto dir = MakeTempDir();
    auto id = PersistentId(kAcct, kCharA, "Hero");
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    svc.BindIdentity(id);

    IngestMapsN(svc, id, {10, 20, 30}, 3);
    const auto& sealed = ActiveCharacter(svc, id);
    Expect(sealed.journey_baselines.maps.state == JourneyBaselineSealState::Sealed, "svc_vet_sealed");
    Expect(sealed.journey_baselines.maps.ids == std::vector<uint32_t>({10, 20, 30}), "svc_vet_ids");
    Expect(sealed.journey_events.empty(), "svc_vet_zero_events");
    Expect(svc.semantic_dirty(), "svc_vet_seal_dirty");
    Expect(svc.Flush(true), "svc_vet_flush");

    QuestProgressService reload;
    reload.Initialize();
    reload.SetStoreDirectory(dir);
    reload.BindIdentity(id);
    const auto& loaded = ActiveCharacter(reload, id);
    Expect(loaded.journey_baselines.maps.state == JourneyBaselineSealState::Sealed, "svc_vet_reload_sealed");
    Expect(loaded.journey_baselines.maps.ids == std::vector<uint32_t>({10, 20, 30}), "svc_vet_reload_ids");
    Expect(loaded.journey_events.empty(), "svc_vet_reload_events");
}

void TestPostSealSingleDeltaAndDedupe()
{
    const auto dir = MakeTempDir();
    auto id = PersistentId(kAcct, kCharA, "Hero");
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    svc.BindIdentity(id);
    IngestMapsN(svc, id, {10, 20}, 3);

    svc.IngestJourneySnapshot(MapsSample(id, {10, 20, 30}, kTs2));
    Expect(ActiveCharacter(svc, id).journey_events.size() == 1, "svc_delta_one");
    Expect(ActiveCharacter(svc, id).journey_events[0].kind == "map_unlock", "svc_delta_kind");
    Expect(ActiveCharacter(svc, id).journey_events[0].map_id == 30u, "svc_delta_id");

    svc.IngestJourneySnapshot(MapsSample(id, {10, 20, 30}, kTs3));
    Expect(ActiveCharacter(svc, id).journey_events.size() == 1, "svc_delta_dedupe");
}

void TestPartialLegacyOrphanNoReflood()
{
    const auto dir = MakeTempDir();
    auto id = PersistentId(kAcct, kCharA, "Hero");

    AccountProgressStore planted;
    planted.account_key = NormalizeAccountKey(kAcct);
    StoredCharacter character;
    character.character_key = id.character_key;
    character.display_name = "Hero";
    JourneyEventRecord legacy;
    legacy.kind = "map_unlock";
    legacy.subject_key = BuildMapSubjectKey(99);
    legacy.observed_at = "2026-01-01T00:00:00.000Z";
    legacy.map_id = 99;
    character.journey_events.push_back(legacy);
    planted.characters[character.character_key] = character;
    Expect(SaveMergedAccountStore(dir, planted.account_key, planted).status == StoreOpStatus::Ok,
        "svc_legacy_plant");

    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    svc.BindIdentity(id);
    IngestMapsN(svc, id, {10, 20}, 3);
    const auto& sealed = ActiveCharacter(svc, id);
    Expect(sealed.journey_baselines.maps.state == JourneyBaselineSealState::Sealed, "svc_legacy_sealed");
    Expect(sealed.journey_baselines.maps.ids == std::vector<uint32_t>({10, 20}), "svc_legacy_live_inventory");
    Expect(sealed.journey_events.size() == 1, "svc_legacy_no_bootstrap");
    Expect(sealed.journey_events[0].map_id == 99u, "svc_legacy_keeps_orphan");

    svc.IngestJourneySnapshot(MapsSample(id, {10, 20, 30}, kTs2));
    Expect(ActiveCharacter(svc, id).journey_events.size() == 2, "svc_legacy_one_delta");
    Expect(ActiveCharacter(svc, id).journey_events[1].map_id == 30u, "svc_legacy_delta_id");
}

void TestFiveIdFamiliesIndependent()
{
    const auto dir = MakeTempDir();
    auto id = PersistentId(kAcct, kCharA, "Hero");
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    svc.BindIdentity(id);

    for (int i = 0; i < 3; ++i) {
        JourneySnapshotResult snapshot;
        snapshot.raw_flood.observed_at = kTs;
        snapshot.raw_flood.maps = MakeRawIdSetFamilyObservation(true, true, {1});
        snapshot.raw_flood.heroes = MakeRawIdSetFamilyObservation(true, true, {2});
        NormalizeRawJourneyFloodObservation(snapshot.raw_flood);
        svc.IngestJourneySnapshot(Stamp(std::move(snapshot), id));
    }
    Expect(ActiveCharacter(svc, id).journey_baselines.maps.state == JourneyBaselineSealState::Sealed,
        "svc_indep_maps_sealed");
    Expect(ActiveCharacter(svc, id).journey_baselines.heroes.state == JourneyBaselineSealState::Sealed,
        "svc_indep_heroes_sealed");
    Expect(ActiveCharacter(svc, id).journey_baselines.character_skills.state
            == JourneyBaselineSealState::Unset,
        "svc_indep_skills_unset");
    Expect(ActiveCharacter(svc, id).journey_baselines.professions.state
            == JourneyBaselineSealState::Unset,
        "svc_indep_prof_unset");
    Expect(ActiveCharacter(svc, id).journey_baselines.vanquish_areas.state
            == JourneyBaselineSealState::Unset,
        "svc_indep_vanq_unset");

    for (int i = 0; i < 3; ++i) {
        JourneySnapshotResult snapshot;
        snapshot.raw_flood.observed_at = kTs2;
        snapshot.raw_flood.character_skills = MakeRawIdSetFamilyObservation(true, true, {5});
        snapshot.raw_flood.professions = MakeRawIdSetFamilyObservation(true, true, {3});
        snapshot.raw_flood.vanquish_areas = MakeRawIdSetFamilyObservation(true, true, {7});
        NormalizeRawJourneyFloodObservation(snapshot.raw_flood);
        svc.IngestJourneySnapshot(Stamp(std::move(snapshot), id));
    }
    Expect(ActiveCharacter(svc, id).journey_baselines.character_skills.state
            == JourneyBaselineSealState::Sealed,
        "svc_indep_skills_sealed");
    Expect(ActiveCharacter(svc, id).journey_baselines.professions.state
            == JourneyBaselineSealState::Sealed,
        "svc_indep_prof_sealed");
    Expect(ActiveCharacter(svc, id).journey_baselines.vanquish_areas.state
            == JourneyBaselineSealState::Sealed,
        "svc_indep_vanq_sealed");
    Expect(ActiveCharacter(svc, id).journey_events.empty(), "svc_indep_zero_events");
}

void TestHardModeFalseSealThenTrue()
{
    const auto dir = MakeTempDir();
    auto id = PersistentId(kAcct, kCharA, "Hero");
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    svc.BindIdentity(id);

    for (int i = 0; i < 3; ++i) {
        JourneySnapshotResult snapshot;
        snapshot.raw_flood.observed_at = kTs;
        snapshot.raw_flood.hard_mode = MakeRawFlagFamilyObservation(true, true, false);
        NormalizeRawJourneyFloodObservation(snapshot.raw_flood);
        svc.IngestJourneySnapshot(Stamp(std::move(snapshot), id));
    }
    Expect(ActiveCharacter(svc, id).journey_baselines.hard_mode.state == JourneyBaselineSealState::Sealed,
        "svc_hm_false_sealed");
    Expect(!ActiveCharacter(svc, id).journey_baselines.hard_mode.unlocked, "svc_hm_false_value");
    Expect(ActiveCharacter(svc, id).journey_events.empty(), "svc_hm_false_no_events");

    JourneySnapshotResult unlocked;
    unlocked.raw_flood.observed_at = kTs2;
    unlocked.raw_flood.hard_mode = MakeRawFlagFamilyObservation(true, true, true);
    NormalizeRawJourneyFloodObservation(unlocked.raw_flood);
    svc.IngestJourneySnapshot(Stamp(std::move(unlocked), id));
    Expect(ActiveCharacter(svc, id).journey_events.size() == 1, "svc_hm_true_one");
    Expect(ActiveCharacter(svc, id).journey_events[0].kind == "hard_mode_unlock", "svc_hm_true_kind");
    Expect(ActiveCharacter(svc, id).journey_baselines.hard_mode.unlocked, "svc_hm_true_unlocked");
}

void TestUnusableEmptyShrinkBadTimestamp()
{
    const auto dir = MakeTempDir();
    auto id = PersistentId(kAcct, kCharA, "Hero");
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    svc.BindIdentity(id);

    IngestMapsN(svc, id, {10, 20}, 2);
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 2, "svc_pre_break_streak");

    JourneySnapshotResult unusable;
    unusable.raw_flood.observed_at = kTs;
    unusable.raw_flood.maps = MakeRawIdSetFamilyObservation(true, false, {10, 20});
    NormalizeRawJourneyFloodObservation(unusable.raw_flood);
    svc.IngestJourneySnapshot(Stamp(std::move(unusable), id));
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "svc_unusable_breaks");
    Expect(ActiveCharacter(svc, id).journey_baselines.maps.state == JourneyBaselineSealState::Unset,
        "svc_unusable_no_seal");

    IngestMapsN(svc, id, {10, 20}, 2);
    JourneySnapshotResult empty;
    empty.raw_flood.observed_at = kTs;
    empty.raw_flood.maps = MakeRawIdSetFamilyObservation(true, true, {});
    NormalizeRawJourneyFloodObservation(empty.raw_flood);
    svc.IngestJourneySnapshot(Stamp(std::move(empty), id));
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "svc_empty_breaks");

    IngestMapsN(svc, id, {10, 20}, 2);
    JourneySnapshotResult shrink;
    shrink.raw_flood.observed_at = kTs;
    shrink.raw_flood.maps = MakeRawIdSetFamilyObservation(true, true, {10});
    NormalizeRawJourneyFloodObservation(shrink.raw_flood);
    svc.IngestJourneySnapshot(Stamp(std::move(shrink), id));
    Expect(ActiveCharacter(svc, id).journey_baselines.maps.state == JourneyBaselineSealState::Unset,
        "svc_shrink_no_seal");

    JourneySnapshotResult bad_ts = MapsSample(id, {10, 20}, kBadTs);
    svc.IngestJourneySnapshot(std::move(bad_ts));
    Expect(ActiveCharacter(svc, id).journey_baselines.maps.state == JourneyBaselineSealState::Unset,
        "svc_bad_ts_no_seal");
}

void TestCharacterAndAccountSwitchFresh()
{
    const auto dir = MakeTempDir();
    auto id_a = PersistentId(kAcct, kCharA, "A");
    auto id_b = PersistentId(kAcct, kCharB, "B");
    auto id_other = PersistentId(kAcct2, kCharA, "Other");
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    svc.BindIdentity(id_a);
    IngestMapsN(svc, id_a, {10, 20}, 2);
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 2, "svc_switch_a_streak");

    svc.BindIdentity(id_b);
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "svc_switch_b_fresh");
    IngestMapsN(svc, id_b, {1}, 1);
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 1, "svc_switch_b_streak");

    svc.BindIdentity(id_a);
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "svc_return_a_no_restore");
    Expect(ActiveCharacter(svc, id_a).journey_baselines.maps.state == JourneyBaselineSealState::Unset,
        "svc_return_a_unset");

    IngestMapsN(svc, id_a, {10, 20}, 2);
    svc.BindIdentity(id_other);
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "svc_acct_fresh");
}

void TestIdentityFenceBlocksMismatchedAndAwaiting()
{
    const auto dir = MakeTempDir();
    auto id_a = PersistentId(kAcct, kCharA, "A");
    auto id_b = PersistentId(kAcct, kCharB, "B");
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    svc.BindIdentity(id_a);

    auto mismatched = MapsSample(id_b, {10, 20});
    svc.IngestJourneySnapshot(std::move(mismatched));
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "svc_fence_mismatch");
    Expect(ActiveCharacter(svc, id_a).journey_baselines.maps.state == JourneyBaselineSealState::Unset,
        "svc_fence_mismatch_unset");

    auto unstamped = MapsSample(id_a, {10, 20});
    unstamped.identity_captured = false;
    unstamped.account_key.clear();
    unstamped.character_key.clear();
    svc.IngestJourneySnapshot(std::move(unstamped));
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "svc_fence_unstamped");

    svc.BindIdentity(id_b);
    svc.BindIdentity(id_a, false, 5, true);
    Expect(svc.awaiting_post_bind_snapshot(), "svc_fence_awaiting");
    svc.IngestJourneySnapshot(MapsSample(id_a, {10, 20}));
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "svc_fence_await_blocks");
}

void TestDetachedFailedFlushPreservesSealedBaseline()
{
    const auto dir = MakeTempDir();
    auto id_a = PersistentId(kAcct, kCharA, "A");
    auto id_b = PersistentId(kAcct, kCharB, "B");
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    svc.SetLockTimeoutMs(100);
    svc.BindIdentity(id_a);

    IngestMapsN(svc, id_a, {10, 20, 30}, 3);
    Expect(ActiveCharacter(svc, id_a).journey_baselines.maps.state == JourneyBaselineSealState::Sealed,
        "svc_detach_pre_sealed");
    Expect(svc.semantic_dirty(), "svc_detach_pre_dirty");
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "svc_detach_candidate_cleared");

    JourneyEventRecord keep;
    keep.kind = "level_up";
    keep.subject_key = BuildLevelSubjectKey(12);
    keep.observed_at = kTs4;
    keep.level = 12;
    JourneySnapshotResult nonflood;
    nonflood.new_events.push_back(keep);
    nonflood.identity_captured = true;
    nonflood.account_key = id_a.account_key;
    nonflood.character_key = id_a.character_key;
    svc.IngestJourneySnapshot(std::move(nonflood));
    Expect(ActiveCharacter(svc, id_a).journey_events.size() == 1, "svc_detach_history_pre");

    const auto t0 = std::chrono::steady_clock::now();
    {
        MutexHold hold(BuildAccountMutexName(NormalizeAccountKey(kAcct)));
        Expect(hold.ok(), "svc_detach_mutex");
        svc.BindIdentity(id_b);
    }
    Expect(svc.detached_session_count() == 1, "svc_detach_retained");
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "svc_detach_b_candidates");

    const auto* detached = [&]() -> const DetachedDirtySession* {
        for (const auto& [key, session] : svc.detached_sessions()) {
            if (key.character_key == id_a.character_key) {
                return &session;
            }
        }
        return nullptr;
    }();
    Expect(detached != nullptr, "svc_detach_found");
    const auto& detached_char = detached->account_store.characters.at(id_a.character_key);
    Expect(detached_char.journey_baselines.maps.state == JourneyBaselineSealState::Sealed,
        "svc_detach_baseline_kept");
    Expect(detached_char.journey_baselines.maps.ids == std::vector<uint32_t>({10, 20, 30}),
        "svc_detach_baseline_ids");
    Expect(detached_char.journey_events.size() == 1, "svc_detach_history_kept");
    Expect(detached_char.journey_events[0].kind == "level_up", "svc_detach_history_kind");

    svc.Tick(t0 + 2s, Wall(20));
    svc.Tick(t0 + 3s, Wall(21));
    Expect(svc.detached_session_count() == 0, "svc_detach_retry_cleared");

    QuestProgressService verify;
    verify.Initialize();
    verify.SetStoreDirectory(dir);
    verify.BindIdentity(id_a);
    Expect(ActiveCharacter(verify, id_a).journey_baselines.maps.state == JourneyBaselineSealState::Sealed,
        "svc_detach_reload_sealed");
    Expect(ActiveCharacter(verify, id_a).journey_baselines.maps.ids
            == std::vector<uint32_t>({10, 20, 30}),
        "svc_detach_reload_ids");
    Expect(ActiveCharacter(verify, id_a).journey_events.size() == 1, "svc_detach_reload_history");
    Expect(verify.JourneyBaselineCandidates()->maps.consecutive_matches == 0,
        "svc_detach_reload_no_candidate");
}

void TestNonFloodEventsUnchangedAlongsideSeal()
{
    const auto dir = MakeTempDir();
    auto id = PersistentId(kAcct, kCharA, "Hero");
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    svc.BindIdentity(id);

    for (int i = 0; i < 3; ++i) {
        JourneySnapshotResult snapshot = MapsSample(id, {10, 20});
        snapshot.level = 8;
        snapshot.observed_map_id = 73;
        if (i == 0) {
            JourneyEventRecord level_up;
            level_up.kind = "level_up";
            level_up.subject_key = BuildLevelSubjectKey(8);
            level_up.observed_at = kTs;
            level_up.level = 8;
            snapshot.new_events.push_back(level_up);
            JourneyEventRecord map_enter;
            map_enter.kind = "map_enter";
            map_enter.subject_key = BuildMapSubjectKey(73);
            map_enter.observed_at = kTs;
            map_enter.map_id = 73;
            snapshot.new_events.push_back(map_enter);
        }
        svc.IngestJourneySnapshot(std::move(snapshot));
    }

    const auto& character = ActiveCharacter(svc, id);
    Expect(character.journey_baselines.maps.state == JourneyBaselineSealState::Sealed,
        "svc_nonflood_sealed");
    Expect(character.journey_events.size() == 2, "svc_nonflood_only_two");
    Expect(character.journey_events[0].kind == "level_up", "svc_nonflood_level");
    Expect(character.journey_events[1].kind == "map_enter", "svc_nonflood_map");
    Expect(character.last_known_level == 8u, "svc_nonflood_level_field");
    Expect(character.last_map_id == 73u, "svc_nonflood_map_field");
}

} // namespace

void RunJourneyBaselineServiceIngestTests()
{
    TestVeteranBootstrapZeroEventsPersistReload();
    TestPostSealSingleDeltaAndDedupe();
    TestPartialLegacyOrphanNoReflood();
    TestFiveIdFamiliesIndependent();
    TestHardModeFalseSealThenTrue();
    TestUnusableEmptyShrinkBadTimestamp();
    TestCharacterAndAccountSwitchFresh();
    TestIdentityFenceBlocksMismatchedAndAwaiting();
    TestDetachedFailedFlushPreservesSealedBaseline();
    TestNonFloodEventsUnchangedAlongsideSeal();
}
