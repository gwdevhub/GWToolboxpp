#include <Modules/QuestProgressDomain.h>
#include <Modules/QuestProgressReducer.h>
#include <Modules/QuestMissionSnapshot.h>
#include <Modules/QuestCharacterJourney.h>

#include "test_assert.h"

#include <string>
#include <vector>

using namespace QuestProgress;

namespace {

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

void TestTerminalAbandonedAbsencePreserved()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:07:10.000Z");
    input2.evidence.push_back({100, EvidenceKind::Abandon});
    auto abandoned = Reduce(input2);
    Expect(abandoned.next.quests.at(100).state == ProgressState::AbandonedObserved, "term_ab_state");
    const auto hist_after_abandon = abandoned.next.quests.at(100).history.size();

    auto input3 = BaseInput(abandoned.next, "2026-07-25T20:07:20.000Z");
    const auto again = Reduce(input3);
    Expect(again.next.quests.at(100).state == ProgressState::AbandonedObserved, "term_ab_absence_keeps");
    Expect(again.next.quests.at(100).confidence == Confidence::Probable, "term_ab_absence_probable");
    Expect(again.appended.empty(), "term_ab_absence_no_append");
    Expect(again.next.quests.at(100).history.size() == hist_after_abandon, "term_ab_history_stable");
    Expect(again.touch_last_observed, "term_ab_touch");

    auto input4 = BaseInput(again.next, "2026-07-25T20:07:30.000Z");
    const auto third = Reduce(input4);
    Expect(third.next.quests.at(100).state == ProgressState::AbandonedObserved, "term_ab_third_keeps");
    Expect(third.appended.empty(), "term_ab_third_no_append");
}

void TestTerminalCompletedAbsencePreserved()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100, true));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:09:10.000Z");
    input2.evidence.push_back({100, EvidenceKind::Reward});
    auto completed = Reduce(input2);
    Expect(completed.next.quests.at(100).state == ProgressState::CompletedObserved, "term_co_state");
    const auto hist = completed.next.quests.at(100).history.size();

    auto input3 = BaseInput(completed.next, "2026-07-25T20:09:20.000Z");
    const auto again = Reduce(input3);
    Expect(again.next.quests.at(100).state == ProgressState::CompletedObserved, "term_co_absence_keeps");
    Expect(again.next.quests.at(100).confidence == Confidence::Probable, "term_co_absence_probable");
    Expect(again.appended.empty(), "term_co_absence_no_append");
    Expect(again.next.quests.at(100).history.size() == hist, "term_co_history_stable");
}

void TestTerminalAbandonedReacquisition()
{
    auto input1 = BaseInput(EmptyCharacter("acct/char-a"));
    input1.observed_quests.push_back(MakeObs(100));
    auto mid = Reduce(input1);

    auto input2 = BaseInput(mid.next, "2026-07-25T20:07:40.000Z");
    input2.evidence.push_back({100, EvidenceKind::Abandon});
    auto abandoned = Reduce(input2);

    auto input3 = BaseInput(abandoned.next, "2026-07-25T20:07:50.000Z");
    input3.observed_quests.push_back(MakeObs(100));
    const auto again = Reduce(input3);
    Expect(again.next.quests.at(100).state == ProgressState::Active, "term_ab_reacquire_active");
    Expect(again.next.quests.at(100).confidence == Confidence::Confirmed, "term_ab_reacquire_confirmed");
    // Semantic key may match the original Active observation; state must still supersede abandon.
    Expect(again.next.quests.at(100).state != ProgressState::AbandonedObserved, "term_ab_reacquire_not_abandoned");
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
    TestTerminalAbandonedAbsencePreserved();
    TestTerminalCompletedAbsencePreserved();
    TestTerminalAbandonedReacquisition();
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

    {
        uint32_t completed[] = {0b101u};
        const MissionBitsetWords completed_nm{completed, 1};
        const auto rows = BuildMissionRecordsFromBitsets(
            completed_nm, {}, {}, {}, "2026-08-30T12:00:00.000Z", nullptr);
        Expect(rows.size() == 2, "mission_bitset_two_maps");
        Expect(MissionBitAt(completed_nm, 0), "mission_bit_zero");
        Expect(MissionBitAt(completed_nm, 2), "mission_bit_two");
        Expect(!MissionBitAt(completed_nm, 1), "mission_bit_one_false");
    }

    {
        std::map<uint32_t, TitleStateRecord> previous;
        TitleStateRecord title12;
        title12.title_id = 12;
        title12.tier_index = 2;
        title12.current_points = 3000;
        title12.last_observed_at = "2026-07-24T10:00:00.000Z";
        previous.emplace(12, title12);

        const std::vector<TitleSnapshotInput> inputs{{12, 3, 4500}};
        const auto first = MergeJourneySnapshot(
            previous,
            std::optional<uint32_t>{4},
            {},
            inputs,
            5,
            "2026-07-25T20:00:00.000Z");
        Expect(first.new_events.size() == 2, "journey_title_and_level_events");
        Expect(first.new_events.at(0).kind == "title_tier", "journey_title_kind");
        Expect(first.new_events.at(0).tier_index == 3, "journey_title_tier");
        Expect(first.new_events.at(1).kind == "level_up", "journey_level_kind");
        Expect(first.new_events.at(1).level == 5, "journey_level_value");

        const auto again = MergeJourneySnapshot(
            first.titles,
            first.level,
            first.new_events,
            inputs,
            5,
            "2026-07-25T21:00:00.000Z");
        Expect(again.new_events.empty(), "journey_idempotent");
    }

    {
        const auto enter = BuildMapEnterEvents(
            std::nullopt,
            73,
            {},
            "2026-07-25T20:00:00.000Z");
        Expect(enter.size() == 1, "map_enter_first");
        Expect(enter.at(0).kind == "map_enter", "map_enter_kind");
        Expect(enter.at(0).map_id == 73, "map_enter_map_id");

        const auto same = BuildMapEnterEvents(
            std::optional<uint32_t>{73},
            73,
            enter,
            "2026-07-25T20:01:00.000Z");
        Expect(same.empty(), "map_enter_same_map");

        const auto next = BuildMapEnterEvents(
            std::optional<uint32_t>{73},
            12,
            enter,
            "2026-07-25T20:02:00.000Z");
        Expect(next.size() == 1, "map_enter_change");
        Expect(next.at(0).map_id == 12, "map_enter_change_id");

        std::map<uint32_t, bool> previous_vq{{10, true}};
        const auto vq = BuildVanquishAreaEvents(
            previous_vq,
            {10, 22},
            {},
            "2026-07-25T20:03:00.000Z");
        Expect(vq.size() == 1, "vanquish_new_only");
        Expect(vq.at(0).kind == "vanquish_area", "vanquish_kind");
        Expect(vq.at(0).map_id == 22, "vanquish_map_id");

        const auto vq_again = BuildVanquishAreaEvents(
            {{10, true}, {22, true}},
            {10, 22},
            vq,
            "2026-07-25T20:04:00.000Z");
        Expect(vq_again.empty(), "vanquish_idempotent");

        const auto skills = BuildNewlySeenIdEvents(
            "skill_unlock",
            JourneyUnlockIdKind::Skill,
            {},
            {42, 99},
            {},
            "2026-07-25T20:05:00.000Z");
        Expect(skills.size() == 2, "skill_unlock_count");
        Expect(skills.at(0).skill_id == 42, "skill_unlock_id");

        const auto account_skills = BuildNewlySeenIdEvents(
            "account_skill_unlock",
            JourneyUnlockIdKind::Skill,
            {},
            {7},
            {},
            "2026-07-25T20:05:30.000Z");
        Expect(account_skills.size() == 1, "account_skill_unlock_count");
        Expect(account_skills.at(0).kind == "account_skill_unlock", "account_skill_unlock_kind");
        Expect(BuildJourneyEventFingerprint(account_skills.at(0)).find("account_skill_unlock")
                != std::string::npos,
            "account_skill_fingerprint");

        const auto heroes = BuildNewlySeenIdEvents(
            "hero_unlock",
            JourneyUnlockIdKind::Hero,
            {{1, true}},
            {1, 7},
            {},
            "2026-07-25T20:06:00.000Z");
        Expect(heroes.size() == 1, "hero_unlock_new_only");
        Expect(heroes.at(0).hero_id == 7, "hero_unlock_id");

        const auto maps_u = BuildNewlySeenIdEvents(
            "map_unlock",
            JourneyUnlockIdKind::Map,
            {},
            {73},
            {},
            "2026-07-25T20:07:00.000Z");
        Expect(maps_u.size() == 1, "map_unlock_count");
        Expect(maps_u.at(0).kind == "map_unlock", "map_unlock_kind");

        const auto hm = BuildHardModeUnlockEvents(
            false,
            true,
            {},
            "2026-07-25T20:08:00.000Z");
        Expect(hm.size() == 1, "hard_mode_unlock_once");
        Expect(BuildHardModeUnlockEvents(true, true, hm, "2026-07-25T20:09:00.000Z").empty(),
            "hard_mode_idempotent");

        const auto profs = BuildNewlySeenIdEvents(
            "profession_unlock",
            JourneyUnlockIdKind::Profession,
            {},
            {5},
            {},
            "2026-07-25T20:10:00.000Z");
        Expect(profs.size() == 1, "profession_unlock_count");
        Expect(profs.at(0).profession_id == 5, "profession_unlock_id");

        const uint32_t bits[1] = {0xFFu};
        Expect(ComputeCartographyCoveragePercent(bits, 1, 8, 1) == 100, "carto_full");
        Expect(ComputeCartographyCoveragePercent(bits, 1, 32, 32) >= 1, "carto_partial");

        const auto carto = BuildCartographyThresholdEvents(
            0, 50, 73, {}, "2026-07-25T20:11:00.000Z");
        Expect(carto.size() == 4, "carto_thresholds_crossed");
        Expect(carto.back().percent == 50, "carto_last_threshold");
        Expect(BuildCartographyThresholdEvents(50, 50, 73, carto, "2026-07-25T20:12:00.000Z").empty(),
            "carto_idempotent");

        const auto dungeon = BuildTimedMapClearEvents(
            "dungeon_complete", 73, {}, "2026-07-25T20:13:00.000Z");
        Expect(dungeon.size() == 1, "dungeon_complete_once");
        Expect(dungeon.at(0).kind == "dungeon_complete", "dungeon_kind");

        const auto sp = BuildAbsoluteThresholdEvents(
            "skill_point_threshold",
            "skill_points",
            0,
            50,
            {1, 10, 25, 50, 100},
            {},
            "2026-07-25T20:14:00.000Z");
        Expect(sp.size() == 4, "skill_point_thresholds");
        Expect(sp.back().amount == 50, "skill_point_last");

        const auto faction = BuildAbsoluteThresholdEvents(
            "faction_threshold",
            "faction:kurzick",
            5000,
            10000,
            {1000, 5000, 10000, 25000},
            {},
            "2026-07-25T20:15:00.000Z");
        Expect(faction.size() == 1, "faction_threshold_new");
        Expect(faction.at(0).amount == 10000, "faction_threshold_amount");

        const auto vq_clear = BuildTimedMapClearEvents(
            "vanquish_complete", 73, {}, "2026-07-25T20:16:00.000Z");
        Expect(vq_clear.size() == 1, "vanquish_complete_once");

        HomSnapshotRecord hom;
        hom.observed_at = "2026-07-25T20:17:00.000Z";
        hom.resilience_points = 3;
        hom.fellowship_points = 1;
        const auto hom_events = BuildHomPointsEvents(std::nullopt, hom, {}, hom.observed_at);
        Expect(hom_events.size() == 2, "hom_points_first_sample");
        Expect(hom_events.at(0).kind == "hom_points", "hom_points_kind");
        Expect(BuildHomPointsEvents(hom, hom, hom_events, "2026-07-25T20:18:00.000Z").empty(),
            "hom_points_idempotent");
        HomSnapshotRecord hom2 = hom;
        hom2.resilience_points = 5;
        const auto hom_up = BuildHomPointsEvents(hom, hom2, hom_events, "2026-07-25T20:19:00.000Z");
        Expect(hom_up.size() == 1 && hom_up.at(0).amount == 5, "hom_points_increase");
    }

    RunBatch2BStoreTests();
    RunBatch2CServiceTests();
    RunAbandonProbeTests();
    RunChatEvidenceTests();
    RunContractExporterTests();

    std::printf("\n%d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed == 0 ? 0 : 1;
}
