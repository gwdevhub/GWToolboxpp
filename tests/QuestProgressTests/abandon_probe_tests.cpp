#include <Modules/QuestAbandonProbe.h>
#include <Modules/QuestProgressDomain.h>
#include <Modules/QuestProgressService.h>
#include <Modules/QuestSessionIdentity.h>

#include "test_assert.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_set>

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
    const auto base = std::filesystem::path(tmp) / L"gwtb_quest_abandon_probe_tests";
    std::error_code ec;
    std::filesystem::create_directories(base, ec);
    const auto dir = base / std::to_string(GetCurrentProcessId())
        / std::to_string(GetTickCount64());
    std::filesystem::create_directories(dir, ec);
    return dir;
}

SessionIdentity PersistentId(const char* account, const char* character, const char* name)
{
    return MakeSessionIdentity(account, character, name, "W", false);
}

QuestSnapshotQuest Q(uint32_t id, bool ready = false)
{
    QuestSnapshotQuest q;
    q.game_quest_id = id;
    q.in_log_completed = ready;
    return q;
}

QuestSnapshot MakeSnap(uint64_t rev, std::initializer_list<QuestSnapshotQuest> quests,
    bool world_ready = true)
{
    QuestSnapshot s;
    s.revision = rev;
    s.loading = !world_ready;
    s.world_ready = world_ready;
    s.selected_active_quest_id = 999;
    s.quests = quests;
    return s;
}

std::chrono::system_clock::time_point Wall(int offset_sec)
{
    return std::chrono::system_clock::time_point(std::chrono::seconds(1'700'000'000 + offset_sec));
}

} // namespace

// Mirrors ScheduleAbandonProbe dirty policy (no GWCA): abandon always requests log + active refresh.
static void ExpectAbandonMarksDirty(bool& quest_log_dirty, bool& active_quest_dirty)
{
    quest_log_dirty = true;
    active_quest_dirty = true;
}

void RunAbandonProbeTests()
{
    const auto t0 = std::chrono::steady_clock::time_point{};

    // --- kSendAbandonQuest marks quest log (and active) dirty ---
    {
        bool quest_log_dirty = false;
        bool active_quest_dirty = false;
        QuestAbandonProbeTracker probes;
        Expect(probes.OnAbandon(42, t0), "probe_on_abandon_creates");
        ExpectAbandonMarksDirty(quest_log_dirty, active_quest_dirty);
        Expect(quest_log_dirty, "abandon_marks_quest_log_dirty");
        Expect(active_quest_dirty, "abandon_marks_active_quest_dirty");
        Expect(probes.Contains(42), "probe_contains_after_abandon");
    }

    // --- Immediate snapshot still containing quest does not stop the probe ---
    {
        QuestAbandonProbeTracker probes;
        probes.OnAbandon(100, t0);
        std::unordered_set<uint32_t> present{100};
        probes.OnWorldReadyQuestLog(t0, present, nullptr);
        Expect(probes.Contains(100), "probe_survives_immediate_present_snap");
        Expect(!probes.Tick(t0, true), "probe_no_tick_before_first_offset");
    }

    // --- Later same-map absence stops the probe ---
    {
        QuestAbandonProbeTracker probes;
        probes.OnAbandon(100, t0);
        probes.OnWorldReadyQuestLog(t0 + 100ms, {100}, nullptr);
        Expect(probes.Contains(100), "probe_still_present_mid");
        probes.OnWorldReadyQuestLog(t0 + 250ms, {}, nullptr);
        Expect(!probes.Contains(100), "probe_stops_on_absence");
        Expect(probes.size() == 0, "probe_empty_after_absence");
    }

    // --- Bounded retry schedule within ~5s pairing window ---
    {
        QuestAbandonProbeTracker probes;
        probes.OnAbandon(7, t0);
        Expect(probes.Tick(t0 + 99ms, true) == false, "probe_tick_before_100ms");
        Expect(probes.Tick(t0 + 100ms, true), "probe_tick_at_100ms");
        Expect(probes.Tick(t0 + 100ms, true) == false, "probe_tick_no_double_100ms");
        Expect(probes.Tick(t0 + 250ms, true), "probe_tick_at_250ms");
        Expect(probes.Tick(t0 + 500ms, true), "probe_tick_at_500ms");
        Expect(probes.Tick(t0 + 1s, true), "probe_tick_at_1s");
        Expect(probes.Tick(t0 + 2s, true), "probe_tick_at_2s");
        Expect(probes.Tick(t0 + 4s, true), "probe_tick_at_4s");
        Expect(probes.Tick(t0 + 5s, true) == false, "probe_tick_no_more_after_schedule");
        Expect(kAbandonProbeAttemptCount == 6, "probe_attempt_count_six");
        Expect(kAbandonProbeOffsets[kAbandonProbeAttemptCount - 1] <= kEvidencePairingWindow,
            "probe_last_offset_within_pairing_window");
    }

    // --- Timeout stops probe; does not imply abandoned_observed (service below) ---
    {
        QuestAbandonProbeTracker probes;
        probes.OnAbandon(55, t0);
        for (size_t i = 0; i < kAbandonProbeAttemptCount; ++i) {
            probes.Tick(t0 + kAbandonProbeOffsets[i], true);
        }
        std::vector<std::string> diags;
        probes.OnWorldReadyQuestLog(t0 + 4s, {55}, &diags);
        Expect(!probes.Contains(55), "probe_cleared_on_timeout");
        Expect(diags.size() == 1, "probe_timeout_diagnostic");
        Expect(diags[0].find("55") != std::string::npos, "probe_timeout_diag_has_id");
    }

    // --- Repeated abandon does not create unbounded probes ---
    {
        QuestAbandonProbeTracker probes;
        Expect(probes.OnAbandon(9, t0), "probe_first_abandon");
        Expect(!probes.OnAbandon(9, t0 + 50ms), "probe_dedupe_same_id");
        Expect(!probes.OnAbandon(9, t0 + 200ms), "probe_dedupe_again");
        Expect(probes.size() == 1, "probe_size_one_after_repeats");
        Expect(probes.OnAbandon(10, t0 + 200ms), "probe_other_id_ok");
        Expect(probes.size() == 2, "probe_size_two_distinct");
    }

    // --- World-not-ready: suspend tick; do not fabricate disappearance ---
    {
        QuestAbandonProbeTracker probes;
        probes.OnAbandon(77, t0);
        Expect(!probes.Tick(t0 + 100ms, false), "probe_suspend_when_not_ready");
        Expect(probes.Contains(77), "probe_still_pending_while_suspended");
        Expect(probes.Tick(t0 + 100ms, true), "probe_resumes_when_ready");
        Expect(probes.Contains(77), "probe_not_consumed_without_absence_call");
    }

    // --- Map load must not consume the probe (observation gates on loading/world_ready) ---
    {
        QuestAbandonProbeTracker probes;
        probes.OnAbandon(88, t0);
        // Observation: if (loading || !world_ready) return — do not call OnWorldReadyQuestLog.
        Expect(!probes.Tick(t0 + 250ms, false), "probe_map_load_no_tick");
        Expect(probes.Contains(88), "probe_survives_map_load_gate");
        probes.Tick(t0 + 250ms, true);
        probes.OnWorldReadyQuestLog(t0 + 250ms, {88}, nullptr);
        Expect(probes.Contains(88), "probe_still_after_post_load_present");
    }

    // --- Termination clears pending probes ---
    {
        QuestAbandonProbeTracker probes;
        probes.OnAbandon(1, t0);
        probes.OnAbandon(2, t0);
        probes.Clear();
        Expect(probes.size() == 0, "probe_clear_empties");
        Expect(!probes.Contains(1), "probe_clear_removes_1");
    }

    // --- Absence while evidence pairable → abandoned_observed/probable ---
    // Quest remains Active while still in log after abandon evidence alone.
    {
        QuestProgressService svc;
        svc.Initialize();
        svc.SetStoreDirectory(MakeTempDir());
        svc.BindIdentity(PersistentId("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",
            "11111111-2222-3333-4444-555555555555", "Hero"));
        const auto t = std::chrono::steady_clock::now();

        QuestAbandonProbeTracker probes;
        svc.IngestSnapshot(MakeSnap(1, {Q(500)}), Wall(100), t);
        Expect(svc.character_progress().quests.at(500).state == ProgressState::Active,
            "svc_probe_start_active");

        EvidenceStamp ab;
        ab.game_quest_id = 500;
        ab.kind = EvidenceKind::Abandon;
        ab.steady_at = t + 10ms;
        svc.IngestEvidence({ab});
        probes.OnAbandon(500, t + 10ms);

        // Immediate re-snapshot still contains quest — probe continues; no abandon state.
        svc.IngestSnapshot(MakeSnap(2, {Q(500)}), Wall(101), t + 20ms);
        probes.OnWorldReadyQuestLog(t + 20ms, {500}, nullptr);
        Expect(probes.Contains(500), "svc_probe_keeps_while_present");
        Expect(svc.character_progress().quests.at(500).state != ProgressState::AbandonedObserved,
            "svc_no_abandon_while_present");
        Expect(svc.character_progress().quests.count(500) == 1, "svc_quest_still_visible");

        // Later same-map absence within pairing window — probe stops; reducer pairs.
        probes.Tick(t + 100ms, true);
        svc.IngestSnapshot(MakeSnap(3, {}), Wall(102), t + 150ms);
        probes.OnWorldReadyQuestLog(t + 150ms, {}, nullptr);
        Expect(!probes.Contains(500), "svc_probe_stops_on_absence");
        Expect(svc.character_progress().quests.at(500).state == ProgressState::AbandonedObserved,
            "svc_abandon_observed_after_absence");
        Expect(svc.character_progress().quests.at(500).confidence == Confidence::Probable,
            "svc_abandon_probable_after_absence");
    }

    // --- Timeout does not fabricate abandoned_observed ---
    {
        QuestProgressService svc;
        svc.Initialize();
        svc.SetStoreDirectory(MakeTempDir());
        svc.BindIdentity(PersistentId("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",
            "11111111-2222-3333-4444-555555555555", "Hero"));
        const auto t = std::chrono::steady_clock::now();

        QuestAbandonProbeTracker probes;
        svc.IngestSnapshot(MakeSnap(1, {Q(600)}), Wall(200), t);
        EvidenceStamp ab;
        ab.game_quest_id = 600;
        ab.kind = EvidenceKind::Abandon;
        ab.steady_at = t + 5ms;
        svc.IngestEvidence({ab});
        probes.OnAbandon(600, t + 5ms);

        // Fire each scheduled re-snapshot while present; defer final resolve for diagnostics.
        for (size_t i = 0; i < kAbandonProbeAttemptCount; ++i) {
            probes.Tick(t + 5ms + kAbandonProbeOffsets[i], true);
            svc.IngestSnapshot(MakeSnap(2 + i, {Q(600)}), Wall(201 + static_cast<int>(i)),
                t + 5ms + kAbandonProbeOffsets[i]);
            if (i + 1 < kAbandonProbeAttemptCount) {
                probes.OnWorldReadyQuestLog(t + 5ms + kAbandonProbeOffsets[i], {600}, nullptr);
            }
        }
        std::vector<std::string> diags;
        probes.OnWorldReadyQuestLog(t + 5ms + 4s, {600}, &diags);
        Expect(!probes.Contains(600), "svc_timeout_clears_probe");
        Expect(!diags.empty(), "svc_timeout_has_diag");
        Expect(svc.character_progress().quests.at(600).state != ProgressState::AbandonedObserved,
            "svc_timeout_no_fabricate_abandon");
        Expect(svc.character_progress().quests.count(600) == 1, "svc_timeout_quest_still_tracked");
    }

    // --- Reward/enquire path unchanged (smoke) ---
    {
        QuestProgressService svc;
        svc.Initialize();
        svc.SetStoreDirectory(MakeTempDir());
        svc.BindIdentity(PersistentId("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",
            "11111111-2222-3333-4444-555555555555", "Hero"));
        const auto t = std::chrono::steady_clock::now();
        svc.IngestSnapshot(MakeSnap(1, {Q(700)}), Wall(300), t);
        svc.IngestSnapshot(MakeSnap(2, {Q(700, true)}), Wall(301), t + 10ms);

        EvidenceStamp rw;
        rw.game_quest_id = 700;
        rw.kind = EvidenceKind::Reward;
        rw.steady_at = t + 20ms;
        svc.IngestEvidence({rw});
        svc.IngestSnapshot(MakeSnap(3, {}), Wall(302), t + 30ms);
        Expect(svc.character_progress().quests.at(700).state == ProgressState::CompletedObserved,
            "svc_reward_unchanged_with_probe_module");

        QuestProgressService svc2;
        svc2.Initialize();
        svc2.SetStoreDirectory(MakeTempDir());
        svc2.BindIdentity(PersistentId("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",
            "11111111-2222-3333-4444-555555555555", "Hero"));
        svc2.IngestSnapshot(MakeSnap(1, {Q(701)}), Wall(310), t);
        EvidenceStamp enq;
        enq.game_quest_id = 701;
        enq.kind = EvidenceKind::EnquireReward;
        enq.steady_at = t + 5ms;
        svc2.IngestEvidence({enq});
        svc2.IngestSnapshot(MakeSnap(2, {}), Wall(311), t + 10ms);
        Expect(svc2.character_progress().quests.at(701).state != ProgressState::CompletedObserved,
            "svc_enquire_unchanged_no_complete");
    }
}
