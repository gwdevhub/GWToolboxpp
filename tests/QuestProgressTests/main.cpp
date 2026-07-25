#include <Modules/QuestProgressDomain.h>
#include <Modules/QuestProgressReducer.h>

#include <cstdio>
#include <string>
#include <vector>

using namespace QuestProgress;

namespace {

int g_failed = 0;
int g_passed = 0;

void Expect(bool condition, const char* name)
{
    if (condition) {
        ++g_passed;
        std::printf("PASS %s\n", name);
    }
    else {
        ++g_failed;
        std::printf("FAIL %s\n", name);
    }
}

CharacterProgress EmptyCharacter(const std::string& key, const std::string& display = {})
{
    CharacterProgress character;
    character.character_key = key;
    character.display_name = display;
    return character;
}

LiveQuestObservation MakeObs(
    uint32_t id,
    bool ready = false,
    std::vector<ObjectiveObservation> objectives = {},
    bool missing = false)
{
    LiveQuestObservation obs;
    obs.game_quest_id = id;
    obs.in_log_completed = ready;
    obs.objectives = std::move(objectives);
    obs.objectives_missing = missing;
    return obs;
}

ObjectiveObservation MakeObj(uint32_t index, bool completed, const char16_t* text)
{
    ObjectiveObservation obj;
    obj.index = index;
    obj.completed = completed;
    obj.encoded_content = text;
    return obj;
}

ReducerInput BaseInput(CharacterProgress previous, std::string at = "2026-07-25T20:00:00.000Z")
{
    ReducerInput input;
    input.previous = std::move(previous);
    input.observed_at_utc = std::move(at);
    return input;
}

bool NoReserved(const CharacterProgress& character)
{
    for (const auto& [id, quest] : character.quests) {
        (void)id;
        if (IsReservedState(quest.state)) {
            return false;
        }
        for (const auto& event : quest.history) {
            if (IsReservedState(event.state)) {
                return false;
            }
        }
    }
    return true;
}

void TestFirstObservation()
{
    auto input = BaseInput(EmptyCharacter("acct/char-a"));
    input.observed_quests.push_back(MakeObs(100));
    const auto out = Reduce(input);
    Expect(out.next.quests.count(100) == 1, "first_observe_creates");
    Expect(out.next.quests.at(100).state == ProgressState::Active, "first_observe_active");
    Expect(out.next.quests.at(100).source == ProgressSource::GameSnapshot, "first_observe_source");
    Expect(out.next.quests.at(100).confidence == Confidence::Confirmed, "first_observe_confidence");
    Expect(out.next.quests.at(100).first_observed_at == input.observed_at_utc, "first_observe_first_at");
    Expect(out.next.quests.at(100).last_observed_at == input.observed_at_utc, "first_observe_last_at");
    Expect(out.appended.size() == 1, "first_observe_one_history");
    Expect(out.semantic_changed, "first_observe_semantic_dirty");
    Expect(NoReserved(out.next), "first_observe_no_reserved");
}

void TestDuplicateIdempotent()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"), "2026-07-25T20:00:00.000Z");
    input1.observed_quests.push_back(MakeObs(100));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:01:00.000Z");
    input2.observed_quests.push_back(MakeObs(100));
    const auto out = Reduce(input2);
    Expect(out.appended.empty(), "dup_no_history");
    Expect(!out.semantic_changed, "dup_no_semantic_dirty");
    Expect(out.touch_last_observed, "dup_touch_last_observed");
    Expect(out.next.quests.at(100).history.size() == 1, "dup_history_size_one");
    Expect(out.next.quests.at(100).last_observed_at == "2026-07-25T20:01:00.000Z", "dup_last_at_updated");
}

void TestObjectiveFingerprintChange()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100, false, {MakeObj(0, false, u"Kill rats")}));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:02:00.000Z");
    input2.observed_quests.push_back(MakeObs(100, false, {MakeObj(0, false, u"Kill more rats")}));
    const auto out = Reduce(input2);
    Expect(out.appended.size() == 1, "obj_text_change_one_append");
    Expect(out.next.quests.at(100).state == ProgressState::ObjectiveProgress, "obj_text_state");
    Expect(out.next.quests.at(100).history.size() == 2, "obj_text_history_len");
    Expect(
        out.next.quests.at(100).history[0].semantic_event_key
            != out.next.quests.at(100).history[1].semantic_event_key,
        "obj_text_keys_differ");
}

void TestReadyForReward()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:03:00.000Z");
    input2.observed_quests.push_back(MakeObs(100, true));
    const auto out = Reduce(input2);
    Expect(out.next.quests.at(100).state == ProgressState::ReadyForReward, "ready_state");
    Expect(out.next.quests.at(100).confidence == Confidence::Confirmed, "ready_confidence");
    Expect(out.appended.size() == 1, "ready_one_append");

    auto input3 = BaseInput(out.next, "2026-07-25T20:04:00.000Z");
    input3.observed_quests.push_back(MakeObs(100, true));
    const auto again = Reduce(input3);
    Expect(again.appended.empty(), "ready_idempotent");
}

void TestBareDisappearance()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100, true));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:05:00.000Z");
    const auto out = Reduce(input2);
    Expect(out.next.quests.at(100).state == ProgressState::Unknown, "bare_remove_unknown");
    Expect(out.next.quests.at(100).confidence == Confidence::Uncertain, "bare_remove_uncertain");
    Expect(!out.next.quests.at(100).completed_at.has_value(), "bare_remove_no_completed_at");
    Expect(out.appended.size() == 1, "bare_remove_one_history");
    Expect(out.appended[0].event_type == HistoryEventType::PresenceLost, "bare_remove_presence_lost");
}

void TestRepeatedDisappearanceIdempotent()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:05:00.000Z");
    auto lost = Reduce(input2);

    auto input3 = BaseInput(lost.next, "2026-07-25T20:06:00.000Z");
    const auto again = Reduce(input3);
    Expect(again.appended.empty(), "repeat_missing_no_append");
    Expect(!again.semantic_changed, "repeat_missing_no_semantic");
    Expect(again.touch_last_observed, "repeat_missing_touch");
    Expect(again.next.quests.at(100).history.size() == 2, "repeat_missing_history_stable");
}

void TestExactAbandon()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:07:00.000Z");
    input2.evidence.push_back({100, EvidenceKind::Abandon});
    const auto out = Reduce(input2);
    Expect(out.next.quests.at(100).state == ProgressState::AbandonedObserved, "abandon_state");
    Expect(out.next.quests.at(100).source == ProgressSource::GameEvent, "abandon_source");
    Expect(out.next.quests.at(100).confidence == Confidence::Probable, "abandon_confidence");
    Expect(out.appended.size() == 1, "abandon_one_append");
}

void TestNonMatchingAbandon()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100));
    input1.observed_quests.push_back(MakeObs(200));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:08:00.000Z");
    input2.observed_quests.push_back(MakeObs(200));
    input2.evidence.push_back({200, EvidenceKind::Abandon});
    const auto out = Reduce(input2);
    Expect(out.next.quests.at(100).state == ProgressState::Unknown, "mismatch_abandon_unknown");
    Expect(out.next.quests.at(100).confidence == Confidence::Uncertain, "mismatch_abandon_uncertain");
    Expect(out.next.quests.at(100).state != ProgressState::AbandonedObserved, "mismatch_not_abandoned");
}

void TestExactReward()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100, true));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:09:00.000Z");
    input2.evidence.push_back({100, EvidenceKind::Reward});
    const auto out = Reduce(input2);
    Expect(out.next.quests.at(100).state == ProgressState::CompletedObserved, "reward_state");
    Expect(out.next.quests.at(100).source == ProgressSource::GameEvent, "reward_source");
    Expect(out.next.quests.at(100).confidence == Confidence::Probable, "reward_confidence");
    Expect(out.next.quests.at(100).completed_at == input2.observed_at_utc, "reward_completed_at");
}

void TestEnquireRewardRejected()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100, true));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:10:00.000Z");
    input2.evidence.push_back({100, EvidenceKind::EnquireReward});
    const auto out = Reduce(input2);
    Expect(out.next.quests.at(100).state == ProgressState::Unknown, "enquire_unknown");
    Expect(out.next.quests.at(100).state != ProgressState::CompletedObserved, "enquire_not_completed");
    Expect(!out.next.quests.at(100).completed_at.has_value(), "enquire_no_completed_at");
}

void TestNonMatchingReward()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100, true));
    input1.observed_quests.push_back(MakeObs(200, true));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:11:00.000Z");
    input2.observed_quests.push_back(MakeObs(200, true));
    input2.evidence.push_back({200, EvidenceKind::Reward});
    const auto out = Reduce(input2);
    Expect(out.next.quests.at(100).state == ProgressState::Unknown, "mismatch_reward_unknown");
    Expect(out.next.quests.at(100).state != ProgressState::CompletedObserved, "mismatch_reward_not_complete");
}

void TestOfflineGapUsesSafeMissingPolicy()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100, true));
    auto mid = Reduce(input1);

    auto with_gap = BaseInput(mid.next, "2026-07-26T10:00:00.000Z");
    with_gap.session_gap = true;
    const auto gap_out = Reduce(with_gap);

    auto without_gap = BaseInput(mid.next, "2026-07-26T10:00:00.000Z");
    without_gap.session_gap = false;
    const auto bare_out = Reduce(without_gap);

    Expect(gap_out.next.quests.at(100).state == ProgressState::Unknown, "gap_unknown");
    Expect(gap_out.next.quests.at(100).confidence == Confidence::Uncertain, "gap_uncertain");
    Expect(gap_out.next.quests.at(100).state != ProgressState::CompletedObserved, "gap_not_completed");
    Expect(gap_out.next.quests.at(100).state != ProgressState::AbandonedObserved, "gap_not_abandoned");
    Expect(gap_out.next.quests.at(100).state == bare_out.next.quests.at(100).state, "gap_same_state_as_bare");
    Expect(gap_out.next.quests.at(100).confidence == bare_out.next.quests.at(100).confidence,
        "gap_same_confidence_as_bare");
    Expect(!gap_out.diagnostics.messages.empty(), "gap_diagnostic_preserved");
}

void TestCharacterIsolation()
{
    auto a1 = BaseInput(EmptyCharacter("acct/char-a", "Hero"));
    a1.observed_quests.push_back(MakeObs(100));
    auto a = Reduce(a1);

    auto b1 = BaseInput(EmptyCharacter("acct/char-b", "Hero"));
    b1.observed_quests.push_back(MakeObs(200));
    auto b = Reduce(b1);

    Expect(a.next.character_key == "acct/char-a", "iso_key_a");
    Expect(b.next.character_key == "acct/char-b", "iso_key_b");
    Expect(a.next.quests.count(100) == 1 && a.next.quests.count(200) == 0, "iso_a_quests");
    Expect(b.next.quests.count(200) == 1 && b.next.quests.count(100) == 0, "iso_b_quests");
    Expect(a.next.display_name == "Hero" && b.next.display_name == "Hero", "iso_same_display_ok");
}

void TestHistoryOrdering()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"), "2026-07-25T20:00:00.000Z");
    input1.observed_quests.push_back(MakeObs(100));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:05:00.000Z");
    input2.observed_quests.push_back(MakeObs(100, false, {MakeObj(0, true, u"Done")}));
    auto out = Reduce(input2);

    Expect(out.next.quests.at(100).history.size() == 2, "order_len");
    Expect(out.next.quests.at(100).history[0].observed_at < out.next.quests.at(100).history[1].observed_at,
        "order_timestamps");
}

void TestSemanticKeyStableAndNoTimestamp()
{
    const auto objectives = std::vector{MakeObj(0, false, u"X")};
    const auto k1 = BuildSemanticEventKey(
        100, HistoryEventType::Observation, ProgressState::ObjectiveProgress,
        ProgressSource::GameSnapshot, Confidence::Confirmed, objectives, EvidenceKind::None);
    const auto k2 = BuildSemanticEventKey(
        100, HistoryEventType::Observation, ProgressState::ObjectiveProgress,
        ProgressSource::GameSnapshot, Confidence::Confirmed, objectives, EvidenceKind::None);
    Expect(k1 == k2, "semantic_key_stable");
    Expect(k1.size() == 16, "semantic_key_hex64");

    auto input1 = BaseInput(EmptyCharacter("acct/char-a"), "2026-07-25T20:00:00.000Z");
    input1.observed_quests.push_back(MakeObs(100, false, objectives));
    auto a = Reduce(input1);
    auto input2 = BaseInput(EmptyCharacter("acct/char-a"), "2026-07-25T21:00:00.000Z");
    input2.observed_quests.push_back(MakeObs(100, false, objectives));
    auto b = Reduce(input2);
    Expect(
        a.next.quests.at(100).history[0].semantic_event_key
            == b.next.quests.at(100).history[0].semantic_event_key,
        "semantic_key_excludes_timestamp");
}

void TestNoSelectedActiveQuestHistory()
{
    // ReducerInput has no selected_active_quest field; selection cannot create history.
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100));
    input1.observed_quests.push_back(MakeObs(200));
    auto mid = Reduce(input1);
    const auto hist_len = mid.next.quests.at(100).history.size()
        + mid.next.quests.at(200).history.size();

    auto input2 = BaseInput(mid.next, "2026-07-25T20:12:00.000Z");
    input2.observed_quests.push_back(MakeObs(100));
    input2.observed_quests.push_back(MakeObs(200));
    const auto out = Reduce(input2);
    Expect(out.appended.empty(), "selection_not_in_input_no_append");
    Expect(
        out.next.quests.at(100).history.size() + out.next.quests.at(200).history.size() == hist_len,
        "selection_history_unchanged");
}

void TestReservedStateInvariants()
{
    Expect(IsReservedState(ProgressState::Available), "reserved_available");
    Expect(IsReservedState(ProgressState::CompletedManual), "reserved_completed_manual");
    Expect(!IsReservedState(ProgressState::Active), "not_reserved_active");

    auto input = BaseInput(EmptyCharacter("acct/char-a"));
    input.observed_quests.push_back(MakeObs(100));
    input.observed_quests.push_back(MakeObs(200, true));
    const auto out = Reduce(input);
    Expect(NoReserved(out.next), "reducer_never_emits_reserved");
}

void TestReservedStateSanitization()
{
    auto previous = EmptyCharacter("acct/char-a");
    ::QuestProgress::QuestProgress poisoned;
    poisoned.game_quest_id = 100;
    poisoned.state = ProgressState::Available;
    poisoned.confidence = Confidence::Confirmed;
    poisoned.first_observed_at = "2026-07-25T19:00:00.000Z";
    poisoned.last_observed_at = "2026-07-25T19:00:00.000Z";
    previous.quests.emplace(100, poisoned);
    previous.last_reduced_at = "2026-07-25T19:00:00.000Z";

    auto input = BaseInput(previous, "2026-07-25T20:00:00.000Z");
    input.observed_quests.push_back(MakeObs(100));
    const auto out = Reduce(input);
    Expect(out.diagnostics.reserved_state_attempt
            || out.next.quests.at(100).state != ProgressState::Available,
        "sanitize_available_not_left");
    Expect(out.next.quests.at(100).state != ProgressState::Available, "sanitize_available_cleared");
    Expect(out.next.quests.at(100).state != ProgressState::CompletedManual, "sanitize_manual_absent");
    Expect(NoReserved(out.next), "sanitize_no_reserved_left");
}

void TestSyntheticFilter()
{
    auto input = BaseInput(EmptyCharacter("acct/char-a"));
    input.observed_quests.push_back(MakeObs(kSyntheticCustomMarkerQuestId));
    input.observed_quests.push_back(MakeObs(100));
    const auto out = Reduce(input);
    Expect(out.next.quests.count(kSyntheticCustomMarkerQuestId) == 0, "synthetic_ignored");
    Expect(out.next.quests.count(100) == 1, "synthetic_other_kept");
}

void TestStaleObservation()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"), "2026-07-25T20:10:00.000Z");
    input1.observed_quests.push_back(MakeObs(100));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:00:00.000Z");
    input2.observed_quests.push_back(MakeObs(100, true));
    const auto out = Reduce(input2);
    Expect(out.diagnostics.rejected_stale, "stale_rejected");
    Expect(out.appended.empty(), "stale_no_append");
    Expect(out.next.quests.at(100).state == ProgressState::Active, "stale_state_unchanged");
    Expect(out.next.quests.at(100).history.size() == mid.next.quests.at(100).history.size(),
        "stale_history_not_reordered");
}

void TestRewardWithoutReadyIsUnknown()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100, false));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:13:00.000Z");
    input2.evidence.push_back({100, EvidenceKind::Reward});
    const auto out = Reduce(input2);
    Expect(out.next.quests.at(100).state == ProgressState::Unknown, "reward_not_ready_unknown");
    Expect(out.next.quests.at(100).state != ProgressState::CompletedObserved, "reward_not_ready_not_complete");
}

void TestLateAbandonEvidence()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"), "2026-07-25T20:00:00.000Z");
    input1.observed_quests.push_back(MakeObs(100));
    auto present = Reduce(input1);

    auto input2 = BaseInput(present.next, "2026-07-25T20:01:00.000Z");
    auto lost = Reduce(input2);
    Expect(lost.next.quests.at(100).state == ProgressState::Unknown, "late_abandon_first_unknown");

    auto input3 = BaseInput(lost.next, "2026-07-25T20:02:00.000Z");
    input3.evidence.push_back({100, EvidenceKind::Abandon});
    const auto upgraded = Reduce(input3);
    Expect(upgraded.next.quests.at(100).state == ProgressState::AbandonedObserved, "late_abandon_upgraded");
    Expect(upgraded.next.quests.at(100).confidence == Confidence::Probable, "late_abandon_probable");
    Expect(upgraded.appended.size() == 1, "late_abandon_one_append");

    auto input4 = BaseInput(upgraded.next, "2026-07-25T20:03:00.000Z");
    input4.evidence.push_back({100, EvidenceKind::Abandon});
    const auto again = Reduce(input4);
    Expect(again.appended.empty(), "late_abandon_dup_no_append");
    Expect(again.next.quests.at(100).history.size() == upgraded.next.quests.at(100).history.size(),
        "late_abandon_history_idempotent");
}

void TestLateRewardEvidence()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"), "2026-07-25T20:00:00.000Z");
    input1.observed_quests.push_back(MakeObs(100, true));
    auto present = Reduce(input1);

    auto input2 = BaseInput(present.next, "2026-07-25T20:01:00.000Z");
    auto lost = Reduce(input2);
    Expect(lost.next.quests.at(100).state == ProgressState::Unknown, "late_reward_first_unknown");
    Expect(HistoryShowsReadyForReward(lost.next.quests.at(100)), "late_reward_history_had_ready");

    auto input3 = BaseInput(lost.next, "2026-07-25T20:02:00.000Z");
    input3.evidence.push_back({100, EvidenceKind::Reward});
    const auto upgraded = Reduce(input3);
    Expect(upgraded.next.quests.at(100).state == ProgressState::CompletedObserved, "late_reward_upgraded");
    Expect(upgraded.next.quests.at(100).confidence == Confidence::Probable, "late_reward_probable");
    Expect(upgraded.next.quests.at(100).completed_at == input3.observed_at_utc, "late_reward_completed_at");
    Expect(upgraded.appended.size() == 1, "late_reward_one_append");

    auto input4 = BaseInput(upgraded.next, "2026-07-25T20:03:00.000Z");
    input4.evidence.push_back({100, EvidenceKind::Reward});
    const auto again = Reduce(input4);
    Expect(again.appended.empty(), "late_reward_dup_no_append");
    Expect(again.next.quests.at(100).history.size() == upgraded.next.quests.at(100).history.size(),
        "late_reward_history_idempotent");
}

void TestLateNonMatchingEvidence()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"), "2026-07-25T20:00:00.000Z");
    input1.observed_quests.push_back(MakeObs(100, true));
    auto present = Reduce(input1);

    auto input2 = BaseInput(present.next, "2026-07-25T20:01:00.000Z");
    auto lost = Reduce(input2);

    auto input3 = BaseInput(lost.next, "2026-07-25T20:02:00.000Z");
    input3.evidence.push_back({200, EvidenceKind::Abandon});
    input3.evidence.push_back({200, EvidenceKind::Reward});
    const auto out = Reduce(input3);
    Expect(out.next.quests.at(100).state == ProgressState::Unknown, "late_mismatch_remains_unknown");
    Expect(out.next.quests.at(100).state != ProgressState::AbandonedObserved, "late_mismatch_not_abandon");
    Expect(out.next.quests.at(100).state != ProgressState::CompletedObserved, "late_mismatch_not_complete");
    Expect(!out.next.quests.at(100).completed_at.has_value(), "late_mismatch_no_completed_at");
}

void TestConflictingEvidenceRejected()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100, true));
    auto mid = Reduce(input1);

    auto abandon_first = BaseInput(mid.next, "2026-07-25T20:20:00.000Z");
    abandon_first.evidence.push_back({100, EvidenceKind::Abandon});
    abandon_first.evidence.push_back({100, EvidenceKind::Reward});
    const auto a = Reduce(abandon_first);

    auto reward_first = BaseInput(mid.next, "2026-07-25T20:20:00.000Z");
    reward_first.evidence.push_back({100, EvidenceKind::Reward});
    reward_first.evidence.push_back({100, EvidenceKind::Abandon});
    const auto b = Reduce(reward_first);

    Expect(a.diagnostics.conflicting_evidence, "conflict_diag_a");
    Expect(b.diagnostics.conflicting_evidence, "conflict_diag_b");
    Expect(a.next.quests.at(100).state == ProgressState::Unknown, "conflict_unknown_a");
    Expect(b.next.quests.at(100).state == ProgressState::Unknown, "conflict_unknown_b");
    Expect(a.next.quests.at(100).state == b.next.quests.at(100).state, "conflict_order_independent_state");
    Expect(a.next.quests.at(100).confidence == Confidence::Uncertain, "conflict_uncertain");
    Expect(a.next.quests.at(100).state != ProgressState::AbandonedObserved, "conflict_not_abandon");
    Expect(a.next.quests.at(100).state != ProgressState::CompletedObserved, "conflict_not_complete");
}

void TestDuplicateIdenticalEvidenceIdempotent()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:21:00.000Z");
    input2.evidence.push_back({100, EvidenceKind::Abandon});
    input2.evidence.push_back({100, EvidenceKind::Abandon});
    const auto out = Reduce(input2);
    Expect(out.next.quests.at(100).state == ProgressState::AbandonedObserved, "dup_evidence_abandon");
    Expect(out.appended.size() == 1, "dup_evidence_one_append");
    Expect(!out.diagnostics.conflicting_evidence, "dup_evidence_not_conflict");
}

void TestDuplicateObjectiveIndexDeterministic()
{
    std::vector<ObjectiveObservation> order_a = {
        MakeObj(0, false, u"B"),
        MakeObj(0, true, u"A"),
    };
    std::vector<ObjectiveObservation> order_b = {
        MakeObj(0, true, u"A"),
        MakeObj(0, false, u"B"),
    };

    const auto key_a = BuildSemanticEventKey(
        100, HistoryEventType::Observation, ProgressState::ObjectiveProgress,
        ProgressSource::GameSnapshot, Confidence::Confirmed, order_a, EvidenceKind::None);
    const auto key_b = BuildSemanticEventKey(
        100, HistoryEventType::Observation, ProgressState::ObjectiveProgress,
        ProgressSource::GameSnapshot, Confidence::Confirmed, order_b, EvidenceKind::None);
    Expect(key_a == key_b, "dup_index_key_order_independent");

    auto input_a = BaseInput(EmptyCharacter("acct/char-a"), "2026-07-25T20:22:00.000Z");
    input_a.observed_quests.push_back(MakeObs(100, false, order_a));
    auto out_a = Reduce(input_a);

    auto input_b = BaseInput(EmptyCharacter("acct/char-a"), "2026-07-25T20:22:00.000Z");
    input_b.observed_quests.push_back(MakeObs(100, false, order_b));
    auto out_b = Reduce(input_b);

    Expect(out_a.diagnostics.duplicate_objective_indices, "dup_index_diag_a");
    Expect(out_b.diagnostics.duplicate_objective_indices, "dup_index_diag_b");
    Expect(
        out_a.next.quests.at(100).history[0].semantic_event_key
            == out_b.next.quests.at(100).history[0].semantic_event_key,
        "dup_index_reduce_key_same");
}

void TestCanonicalTimestampContract()
{
    Expect(IsCanonicalUtcTimestamp("2026-07-25T20:00:00.000Z"), "ts_canonical_ok");
    Expect(!IsCanonicalUtcTimestamp("2026-07-25T20:00:00Z"), "ts_reject_no_millis");
    Expect(!IsCanonicalUtcTimestamp("2026-7-25T20:00:00.000Z"), "ts_reject_unpadded");
    Expect(!IsCanonicalUtcTimestamp("2026-07-25T20:00:00.000+00:00"), "ts_reject_offset");
    Expect(!IsCanonicalUtcTimestamp(""), "ts_reject_empty");

    auto input1 = BaseInput(EmptyCharacter("acct/char-a"), "2026-07-25T20:10:00.000Z");
    input1.observed_quests.push_back(MakeObs(100));
    auto mid = Reduce(input1);

    auto malformed = BaseInput(mid.next, "2026-07-25T21:00:00Z");
    malformed.observed_quests.push_back(MakeObs(100, true));
    const auto out = Reduce(malformed);
    Expect(out.diagnostics.invalid_timestamp, "ts_malformed_rejected");
    Expect(out.appended.empty(), "ts_malformed_no_append");
    Expect(out.next.quests.at(100).state == ProgressState::Active, "ts_malformed_no_overwrite");
    Expect(out.next.last_reduced_at == "2026-07-25T20:10:00.000Z", "ts_malformed_last_reduced_unchanged");
}

} // namespace

int main()
{
    TestFirstObservation();
    TestDuplicateIdempotent();
    TestObjectiveFingerprintChange();
    TestReadyForReward();
    TestBareDisappearance();
    TestRepeatedDisappearanceIdempotent();
    TestExactAbandon();
    TestNonMatchingAbandon();
    TestExactReward();
    TestEnquireRewardRejected();
    TestNonMatchingReward();
    TestOfflineGapUsesSafeMissingPolicy();
    TestCharacterIsolation();
    TestHistoryOrdering();
    TestSemanticKeyStableAndNoTimestamp();
    TestNoSelectedActiveQuestHistory();
    TestReservedStateInvariants();
    TestReservedStateSanitization();
    TestSyntheticFilter();
    TestStaleObservation();
    TestRewardWithoutReadyIsUnknown();
    TestLateAbandonEvidence();
    TestLateRewardEvidence();
    TestLateNonMatchingEvidence();
    TestConflictingEvidenceRejected();
    TestDuplicateIdenticalEvidenceIdempotent();
    TestDuplicateObjectiveIndexDeterministic();
    TestCanonicalTimestampContract();

    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
