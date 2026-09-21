#include <Modules/QuestProgressContractExporter.h>
#include <Modules/QuestProgressDomain.h>
#include <Modules/QuestProgressReducer.h>

#include "test_assert.h"

#include <string>

using namespace QuestProgress;

namespace {

AccountProgressStore MakeSampleStore()
{
    AccountProgressStore store;
    store.account_key = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";

    StoredCharacter character;
    character.character_key = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee/11111111-2222-3333-4444-555555555555";
    character.display_name = "Test Hero";
    character.is_pre_searing = false;
    character.is_pvp = false;
    character.experience_total = 12345;
    character.last_known_level = 20;
    character.skill_points_earned = 150;
    FactionTotalsRecord factions;
    factions.kurzick = 10000;
    factions.luxon = 5000;
    factions.balthazar = 2000;
    factions.imperial = 8000;
    character.faction_totals = factions;
    character.profession = "5";
    character.secondary_profession = "1";
    HomSnapshotRecord hom;
    hom.hom_code = "abc123";
    hom.observed_at = "2026-07-25T16:00:00.000Z";
    hom.resilience_points = 3;
    hom.fellowship_points = 2;
    hom.resilience_dedicated = {1, 0, 1};
    character.hall_of_monuments = hom;
    JourneyEventRecord map_enter;
    map_enter.kind = "map_enter";
    map_enter.subject_key = "map:73";
    map_enter.observed_at = "2026-07-25T17:00:00.000Z";
    map_enter.map_id = 73;
    character.journey_events.push_back(map_enter);

    MissionRecord mission;
    mission.map_id = 73;
    mission.completed_normal = true;
    mission.bonus_normal = true;
    mission.last_observed_at = "2026-07-25T18:00:00.000Z";
    character.missions.emplace(mission.map_id, mission);

    ::QuestProgress::QuestProgress quest;
    quest.game_quest_id = 42;
    quest.state = ProgressState::ObjectiveProgress;
    quest.source = ProgressSource::GameSnapshot;
    quest.confidence = Confidence::Confirmed;
    quest.first_observed_at = "2026-07-25T19:00:00.000Z";
    quest.last_observed_at = "2026-07-25T20:00:00.000Z";

    ObjectiveObservation obj0;
    obj0.index = 1;
    obj0.completed = false;
    obj0.encoded_content = u"second";
    ObjectiveObservation obj1;
    obj1.index = 0;
    obj1.completed = true;
    obj1.encoded_content = u"first";
    quest.objectives = {obj0, obj1};
    NormalizeObjectives(quest.objectives);

    QuestHistoryEvent hist;
    hist.game_quest_id = 42;
    hist.event_type = HistoryEventType::Observation;
    hist.state = ProgressState::Active;
    hist.source = ProgressSource::GameEvent;
    hist.confidence = Confidence::Confirmed;
    hist.observed_at = "2026-07-25T19:00:00.000Z";
    hist.semantic_event_key = BuildSemanticEventKey(
        hist.game_quest_id,
        hist.event_type,
        hist.state,
        hist.source,
        hist.confidence,
        {},
        EvidenceKind::None);
    hist.evidence_kind = EvidenceKind::None;
    quest.history.push_back(hist);

    QuestHistoryEvent abandon;
    abandon.game_quest_id = 42;
    abandon.event_type = HistoryEventType::Abandoned;
    abandon.state = ProgressState::AbandonedObserved;
    abandon.source = ProgressSource::GameEvent;
    abandon.confidence = Confidence::Probable;
    abandon.observed_at = "2026-07-25T19:30:00.000Z";
    abandon.evidence_kind = EvidenceKind::Abandon;
    abandon.semantic_event_key = BuildSemanticEventKey(
        abandon.game_quest_id,
        abandon.event_type,
        abandon.state,
        abandon.source,
        abandon.confidence,
        {},
        abandon.evidence_kind);
    quest.history.push_back(abandon);

    // Synthetic custom marker must be skipped.
    ::QuestProgress::QuestProgress synthetic;
    synthetic.game_quest_id = kSyntheticCustomMarkerQuestId;
    synthetic.state = ProgressState::Active;
    synthetic.source = ProgressSource::GameSnapshot;
    synthetic.confidence = Confidence::Confirmed;
    synthetic.last_observed_at = "2026-07-25T20:00:00.000Z";

    character.quests.emplace(quest.game_quest_id, quest);
    character.quests.emplace(synthetic.game_quest_id, synthetic);
    store.characters.emplace(character.character_key, character);
    return store;
}

void TestInvalidExportedAt()
{
    ContractExportOptions options;
    options.exported_at_utc = "2026-07-25T20:00:00Z"; // missing millis
    const auto result = ExportAccountStoreToContractV1({}, options);
    Expect(result.status == ContractExportStatus::InvalidOptions, "export_reject_bad_exported_at");
    Expect(result.utf8_json.empty(), "export_reject_clears_json");
}

void TestEnvelopeAndMapping()
{
    ContractExportOptions options;
    options.exported_at_utc = "2026-07-25T20:00:00.000Z";
    options.producer_version = "test-producer-0.0.0";

    const auto result = ExportAccountStoreToContractV1(MakeSampleStore(), options);
    Expect(result.status == ContractExportStatus::Ok, "export_status_ok");
    Expect(result.diagnostics.characters_exported == 1, "export_one_character");
    Expect(result.diagnostics.quests_exported == 1, "export_skips_synthetic");
    Expect(result.diagnostics.quests_skipped == 1, "export_counts_skipped_synthetic");
    Expect(result.diagnostics.history_events_exported == 2, "export_history_count");

    const auto& json = result.utf8_json;
    Expect(json.find("\"contract\": \"guild-wars-quest-progress\"") != std::string::npos
            || json.find("\"contract\":\"guild-wars-quest-progress\"") != std::string::npos,
        "export_contract_id");
    Expect(json.find("\"contractVersion\": 1") != std::string::npos
            || json.find("\"contractVersion\":1") != std::string::npos,
        "export_contract_version");
    Expect(json.find("GWToolboxpp") != std::string::npos, "export_producer_name");
    Expect(json.find("test-producer-0.0.0") != std::string::npos, "export_producer_version");
    Expect(json.find("accountKey") == std::string::npos, "export_no_account_key");
    Expect(json.find("storeFormat") == std::string::npos, "export_no_store_format");
    Expect(json.find("encodedContentHex") == std::string::npos, "export_no_encoded_hex");
    Expect(json.find("semanticEventKey") == std::string::npos, "export_no_semantic_key_field");
    Expect(json.find("\"observedAt\"") != std::string::npos, "export_has_observed_at");
    Expect(json.find("\"observedAt\": \"2026-07-25T20:00:00.000Z\"") != std::string::npos
            || json.find("\"observedAt\":\"2026-07-25T20:00:00.000Z\"") != std::string::npos,
        "export_quest_observed_at");
    Expect(json.find("\"lastObservedAt\": \"2026-07-25T18:00:00.000Z\"") != std::string::npos
            || json.find("\"lastObservedAt\":\"2026-07-25T18:00:00.000Z\"") != std::string::npos,
        "export_mission_last_observed_at");
    Expect(json.find("\"objectiveIndex\"") != std::string::npos, "export_objective_index");
    Expect(json.find("\"primaryProfession\"") != std::string::npos, "export_primary_profession");
    Expect(json.find("\"secondaryProfession\"") != std::string::npos, "export_secondary_profession");
    Expect(json.find("\"isPvp\"") != std::string::npos, "export_is_pvp");
    Expect(json.find("\"experienceTotal\"") != std::string::npos, "export_experience_total");
    Expect(json.find("\"level\"") != std::string::npos, "export_level");
    Expect(json.find("\"skillPointsEarned\"") != std::string::npos, "export_skill_points_earned");
    Expect(json.find("\"factionTotals\"") != std::string::npos, "export_faction_totals");
    Expect(json.find("\"hallOfMonuments\"") != std::string::npos, "export_hall_of_monuments");
    Expect(json.find("\"resiliencePoints\"") != std::string::npos, "export_hom_resilience");
    Expect(json.find("\"resilienceDedicated\"") != std::string::npos, "export_hom_resilience_dedicated");
    Expect(json.find("\"map_enter\"") != std::string::npos, "export_map_enter_kind");
    Expect(json.find("\"mapId\"") != std::string::npos, "export_mission_map_id");
    Expect(json.find("\"isCompleted\"") != std::string::npos, "export_is_completed");
    Expect(json.find("\"eventId\"") != std::string::npos, "export_event_id");
    Expect(json.find("\"evidenceKind\"") != std::string::npos, "export_evidence_payload");
    Expect(json.find("4053") == std::string::npos && json.find("0xfdd") == std::string::npos,
        "export_no_synthetic_id_literal");
    // Objectives sorted: index 0 before index 1 in JSON object order within array.
    const auto idx0 = json.find("\"objectiveIndex\": 0");
    const auto idx0_alt = json.find("\"objectiveIndex\":0");
    const auto pos0 = idx0 != std::string::npos ? idx0 : idx0_alt;
    const auto idx1 = json.find("\"objectiveIndex\": 1");
    const auto idx1_alt = json.find("\"objectiveIndex\":1");
    const auto pos1 = idx1 != std::string::npos ? idx1 : idx1_alt;
    Expect(pos0 != std::string::npos && pos1 != std::string::npos && pos0 < pos1, "export_objectives_sorted");
}

void TestEmptyStoreOk()
{
    ContractExportOptions options;
    options.exported_at_utc = "2026-07-25T20:00:00.000Z";
    const auto result = ExportAccountStoreToContractV1({}, options);
    Expect(result.status == ContractExportStatus::Ok, "export_empty_ok");
    Expect(result.diagnostics.characters_exported == 0, "export_empty_chars");
    Expect(result.utf8_json.find("\"characters\": []") != std::string::npos
            || result.utf8_json.find("\"characters\":[]") != std::string::npos,
        "export_empty_characters_array");
}

AccountProgressStore MakeTwoCharacterStore()
{
    auto store = MakeSampleStore();
    StoredCharacter ghost;
    ghost.character_key = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee/66666666-7777-8888-9999-aaaaaaaaaaaa";
    ghost.display_name = "Test Hero"; // same displayName, different characterKey
    ::QuestProgress::QuestProgress q;
    q.game_quest_id = 99;
    q.state = ProgressState::Active;
    q.source = ProgressSource::GameSnapshot;
    q.confidence = Confidence::Confirmed;
    q.last_observed_at = "2026-07-25T20:00:00.000Z";
    ghost.quests.emplace(99, q);
    store.characters.emplace(ghost.character_key, ghost);

    StoredCharacter invalid;
    invalid.character_key = "";
    invalid.display_name = "Ghost";
    invalid.quests.emplace(1, q);
    store.characters.emplace("", invalid);
    return store;
}

void TestBoundCharacterExportOnly()
{
    ContractExportOptions options;
    options.exported_at_utc = "2026-07-25T20:00:00.000Z";
    options.bound_character_key =
        "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee/11111111-2222-3333-4444-555555555555";

    const auto result = ExportAccountStoreToContractV1(MakeTwoCharacterStore(), options);
    Expect(result.status == ContractExportStatus::Ok, "export_bound_ok");
    Expect(result.diagnostics.characters_exported == 1, "export_bound_one_char");
    Expect(result.diagnostics.characters_skipped >= 1, "export_bound_skips_others");
    Expect(result.diagnostics.quests_exported == 1, "export_bound_one_quest");
    Expect(result.utf8_json.find("66666666-7777-8888-9999-aaaaaaaaaaaa") == std::string::npos,
        "export_bound_no_other_uuid");
    Expect(result.utf8_json.find("11111111-2222-3333-4444-555555555555") != std::string::npos,
        "export_bound_has_target_uuid");
}

void TestInvalidCharacterKeySkipped()
{
    AccountProgressStore store;
    StoredCharacter bad;
    bad.character_key = "not-a-valid-key";
    bad.display_name = "Bad";
    ::QuestProgress::QuestProgress q;
    q.game_quest_id = 1;
    q.state = ProgressState::Active;
    q.source = ProgressSource::GameSnapshot;
    q.confidence = Confidence::Confirmed;
    q.last_observed_at = "2026-07-25T20:00:00.000Z";
    bad.quests.emplace(1, q);
    store.characters.emplace(bad.character_key, bad);

    ContractExportOptions options;
    options.exported_at_utc = "2026-07-25T20:00:00.000Z";
    const auto result = ExportAccountStoreToContractV1(store, options);
    Expect(result.status == ContractExportStatus::Ok, "export_invalid_key_store_ok");
    Expect(result.diagnostics.characters_exported == 0, "export_invalid_key_none");
    Expect(result.diagnostics.characters_skipped == 1, "export_invalid_key_skipped");
}

void TestBoundMissingCharacterFails()
{
    ContractExportOptions options;
    options.exported_at_utc = "2026-07-25T20:00:00.000Z";
    options.bound_character_key =
        "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee/99999999-9999-9999-9999-999999999999";
    const auto result = ExportAccountStoreToContractV1(MakeSampleStore(), options);
    Expect(result.status == ContractExportStatus::InvalidOptions, "export_bound_missing_fail");
    Expect(result.utf8_json.empty(), "export_bound_missing_no_json");
}

} // namespace

void RunContractExporterTests()
{
    TestInvalidExportedAt();
    TestEnvelopeAndMapping();
    TestEmptyStoreOk();
    TestBoundCharacterExportOnly();
    TestInvalidCharacterKeySkipped();
    TestBoundMissingCharacterFails();
}
