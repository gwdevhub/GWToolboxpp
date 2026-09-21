#include <Modules/QuestJourneyBaselineCandidates.h>
#include <Modules/QuestJourneyBaselineTransition.h>
#include <Modules/QuestProgressDomain.h>
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

namespace {

const char* kAcct = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
const char* kCharA = "11111111-2222-3333-4444-555555555555";
const char* kCharB = "66666666-7777-8888-9999-aaaaaaaaaaaa";
const char* kAcct2 = "bbbbbbbb-cccc-dddd-eeee-ffffffffffff";

std::filesystem::path MakeTempDir()
{
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    const auto base = std::filesystem::path(tmp) / L"gwtb_quest_baseline_candidate_tests";
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

SessionIdentity EphemeralId(const char* account, const char* name)
{
    return MakeSessionIdentity(account, "00000000-0000-0000-0000-000000000000", name, "W", false);
}

void SeedMapsStreak(CharacterJourneyBaselineCandidates& candidates, uint32_t consecutive)
{
    candidates.maps.active = true;
    candidates.maps.ids = {10, 20};
    candidates.maps.seen_union = {10, 20};
    candidates.maps.consecutive_matches = consecutive;
}

void SeedHeroesStreak(CharacterJourneyBaselineCandidates& candidates, uint32_t consecutive)
{
    candidates.heroes.active = true;
    candidates.heroes.ids = {3};
    candidates.heroes.seen_union = {3};
    candidates.heroes.consecutive_matches = consecutive;
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

void TestOwnerRequiresPersistentKeys()
{
    JourneyBaselineCandidateOwner owner;
    Expect(owner.Active() == nullptr, "owner_start_inactive");

    SessionIdentity unbound{};
    owner.OpenForPersistentIdentity(unbound);
    Expect(owner.Active() == nullptr, "owner_unbound_inactive");

    owner.OpenForPersistentIdentity(EphemeralId(kAcct, "Temp"));
    Expect(owner.Active() == nullptr, "owner_ephemeral_inactive");

    auto id = PersistentId(kAcct, kCharA, "Hero");
    owner.OpenForPersistentIdentity(id);
    auto* active = owner.Active();
    Expect(active != nullptr, "owner_persistent_active");
    Expect(owner.account_key() == id.account_key, "owner_account_key");
    Expect(owner.character_key() == id.character_key, "owner_character_key");
    SeedMapsStreak(*active, 2);
    Expect(active->maps.consecutive_matches == 2, "owner_seeded");

    owner.OpenForPersistentIdentity(id);
    Expect(owner.Active()->maps.consecutive_matches == 2, "owner_same_open_preserves");

    owner.Clear();
    Expect(owner.Active() == nullptr, "owner_clear_inactive");
}

void TestSameCharacterRebindAndMapLoadPreserve()
{
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(MakeTempDir());
    auto id_a = PersistentId(kAcct, kCharA, "Hero");
    svc.BindIdentity(id_a);
    auto* candidates = svc.MutableJourneyBaselineCandidates();
    Expect(candidates != nullptr, "rebind_open");
    SeedMapsStreak(*candidates, 2);

    svc.BindIdentity(PersistentId(kAcct, kCharA, "Hero Renamed"));
    Expect(svc.JourneyBaselineCandidates() != nullptr, "rebind_still_active");
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 2, "rebind_preserves_streak");
    Expect(svc.identity().display_name == "Hero Renamed", "rebind_metadata");

    svc.BindIdentity(id_a);
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 2, "mapload_preserves_streak");
}

void TestForceSessionGapResetsSameCharacter()
{
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(MakeTempDir());
    auto id_a = PersistentId(kAcct, kCharA, "Hero");
    svc.BindIdentity(id_a);
    SeedMapsStreak(*svc.MutableJourneyBaselineCandidates(), 2);

    svc.BindIdentity(id_a, true);
    Expect(svc.JourneyBaselineCandidates() != nullptr, "gap_reopen");
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "gap_clears_streak");
    Expect(!svc.JourneyBaselineCandidates()->maps.active, "gap_maps_inactive");
}

void TestCharacterSwitchAndReturnFresh()
{
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(MakeTempDir());
    auto id_a = PersistentId(kAcct, kCharA, "HeroA");
    auto id_b = PersistentId(kAcct, kCharB, "HeroB");
    svc.BindIdentity(id_a);
    SeedMapsStreak(*svc.MutableJourneyBaselineCandidates(), 2);

    svc.BindIdentity(id_b);
    Expect(svc.JourneyBaselineCandidates() != nullptr, "switch_b_active");
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "switch_b_fresh");
    SeedMapsStreak(*svc.MutableJourneyBaselineCandidates(), 1);

    svc.BindIdentity(id_a);
    Expect(svc.JourneyBaselineCandidates() != nullptr, "return_a_active");
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "return_a_no_restore");
    Expect(!svc.JourneyBaselineCandidates()->maps.active, "return_a_inactive");
}

void TestAccountSwitchClears()
{
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(MakeTempDir());
    svc.BindIdentity(PersistentId(kAcct, kCharA, "Hero"));
    SeedMapsStreak(*svc.MutableJourneyBaselineCandidates(), 2);

    svc.BindIdentity(PersistentId(kAcct2, kCharA, "Other"));
    Expect(svc.JourneyBaselineCandidates() != nullptr, "acct_switch_active");
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "acct_switch_fresh");
}

void TestLogoutAndEphemeralClear()
{
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(MakeTempDir());
    auto id_a = PersistentId(kAcct, kCharA, "Hero");
    svc.BindIdentity(id_a);
    SeedMapsStreak(*svc.MutableJourneyBaselineCandidates(), 2);

    svc.UnbindIdentity();
    Expect(svc.JourneyBaselineCandidates() == nullptr, "logout_no_candidates");

    svc.BindIdentity(id_a);
    SeedMapsStreak(*svc.MutableJourneyBaselineCandidates(), 2);
    svc.BindIdentity(EphemeralId(kAcct, "Temp"));
    Expect(svc.JourneyBaselineCandidates() == nullptr, "ephemeral_no_candidates");

    svc.BindIdentity(id_a);
    Expect(svc.JourneyBaselineCandidates() != nullptr, "post_ephemeral_open");
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "post_ephemeral_fresh");
}

void TestTerminateAndReinitFresh()
{
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(MakeTempDir());
    auto id_a = PersistentId(kAcct, kCharA, "Hero");
    svc.BindIdentity(id_a);
    SeedMapsStreak(*svc.MutableJourneyBaselineCandidates(), 2);

    svc.SignalTerminate();
    svc.Terminate();
    Expect(svc.JourneyBaselineCandidates() == nullptr, "terminate_cleared");

    svc.Initialize();
    svc.SetStoreDirectory(MakeTempDir());
    Expect(svc.JourneyBaselineCandidates() == nullptr, "reinit_unbound_none");
    svc.BindIdentity(id_a);
    Expect(svc.JourneyBaselineCandidates() != nullptr, "reinit_open");
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "reinit_fresh");
}

void TestFlushFailureDetachDoesNotPreserveCandidates()
{
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(MakeTempDir());
    svc.SetLockTimeoutMs(100);
    auto id_a = PersistentId(kAcct, kCharA, "Hero");
    auto id_b = PersistentId(kAcct, kCharB, "HeroB");
    svc.BindIdentity(id_a);
    SeedMapsStreak(*svc.MutableJourneyBaselineCandidates(), 2);

    QuestSnapshot snap;
    snap.revision = 1;
    snap.loading = false;
    snap.world_ready = true;
    snap.identity_captured = true;
    snap.account_key = id_a.account_key;
    snap.character_key = id_a.character_key;
    QuestSnapshotQuest q;
    q.game_quest_id = 100;
    snap.quests = {q};
    const auto t0 = std::chrono::steady_clock::now();
    svc.IngestSnapshot(
        snap, std::chrono::system_clock::time_point(std::chrono::seconds(1'700'000'000)), t0);
    Expect(svc.semantic_dirty(), "detach_dirty");

    {
        MutexHold hold(BuildAccountMutexName(NormalizeAccountKey(kAcct)));
        Expect(hold.ok(), "detach_mutex");
        svc.BindIdentity(id_b);
    }
    Expect(svc.detached_session_count() == 1, "detach_retained");
    Expect(svc.JourneyBaselineCandidates() != nullptr, "detach_b_active");
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "detach_b_fresh");

    svc.BindIdentity(id_a);
    Expect(svc.JourneyBaselineCandidates() != nullptr, "detach_return_a");
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "detach_no_restore");
}

void TestFamilyIsolation()
{
    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(MakeTempDir());
    svc.BindIdentity(PersistentId(kAcct, kCharA, "Hero"));
    auto* candidates = svc.MutableJourneyBaselineCandidates();
    SeedMapsStreak(*candidates, 2);
    SeedHeroesStreak(*candidates, 1);

    Expect(candidates->maps.consecutive_matches == 2, "family_maps");
    Expect(candidates->heroes.consecutive_matches == 1, "family_heroes");
    Expect(candidates->character_skills.consecutive_matches == 0, "family_skills_untouched");
    Expect(!candidates->character_skills.active, "family_skills_inactive");
    Expect(!candidates->hard_mode.active, "family_hard_mode_inactive");

    IdSetJourneyBaseline baseline{};
    auto step = TransitionIdSetJourneyBaseline(
        MakeRawIdSetFamilyObservation(true, false, {10, 20}),
        baseline,
        candidates->maps,
        {},
        "map_unlock",
        JourneyUnlockIdKind::Map,
        "2026-09-13T10:00:00.000Z");
    candidates->maps = step.candidate;
    Expect(candidates->maps.consecutive_matches == 0, "family_maps_streak_reset");
    Expect(candidates->heroes.consecutive_matches == 1, "family_heroes_preserved");
}

void TestSealedStoreUntouchedByCandidateLifecycle()
{
    const auto dir = MakeTempDir();
    auto id_a = PersistentId(kAcct, kCharA, "Hero");
    auto id_b = PersistentId(kAcct, kCharB, "HeroB");

    AccountProgressStore planted;
    planted.account_key = NormalizeAccountKey(kAcct);
    StoredCharacter character;
    character.character_key = id_a.character_key;
    character.display_name = "Hero";
    character.journey_baselines.maps = {JourneyBaselineSealState::Sealed, {1, 2}};
    planted.characters[character.character_key] = character;
    const auto saved = SaveMergedAccountStore(dir, planted.account_key, planted);
    Expect(saved.status == StoreOpStatus::Ok, "sealed_plant_ok");

    QuestProgressService svc;
    svc.Initialize();
    svc.SetStoreDirectory(dir);
    svc.BindIdentity(id_a);
    Expect(svc.account_store().characters.at(id_a.character_key).journey_baselines.maps.state
            == JourneyBaselineSealState::Sealed,
        "sealed_loaded");
    SeedMapsStreak(*svc.MutableJourneyBaselineCandidates(), 2);

    svc.BindIdentity(id_b);
    svc.BindIdentity(id_a);
    Expect(svc.account_store().characters.at(id_a.character_key).journey_baselines.maps.state
            == JourneyBaselineSealState::Sealed,
        "sealed_state_preserved");
    Expect(svc.account_store().characters.at(id_a.character_key).journey_baselines.maps.ids
            == std::vector<uint32_t>({1, 2}),
        "sealed_ids_preserved");
    Expect(svc.JourneyBaselineCandidates()->maps.consecutive_matches == 0, "sealed_candidates_fresh");
}

} // namespace

void RunJourneyBaselineCandidateLifecycleTests()
{
    TestOwnerRequiresPersistentKeys();
    TestSameCharacterRebindAndMapLoadPreserve();
    TestForceSessionGapResetsSameCharacter();
    TestCharacterSwitchAndReturnFresh();
    TestAccountSwitchClears();
    TestLogoutAndEphemeralClear();
    TestTerminateAndReinitFresh();
    TestFlushFailureDetachDoesNotPreserveCandidates();
    TestFamilyIsolation();
    TestSealedStoreUntouchedByCandidateLifecycle();
}
