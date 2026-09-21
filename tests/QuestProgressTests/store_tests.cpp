#include <Modules/QuestProgressJsonCodec.h>
#include <Modules/QuestProgressStore.h>
#include <Modules/QuestCharacterJourney.h>
#include <Utils/AtomicJsonFile.h>

#include "test_assert.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
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

std::filesystem::path MakeTempDir()
{
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    const auto base = std::filesystem::path(tmp) / L"gwtb_quest_progress_tests";
    std::error_code ec;
    std::filesystem::create_directories(base, ec);
    static std::atomic<uint32_t> seq{0};
    const auto dir = base / std::to_string(GetCurrentProcessId())
        / (std::to_string(GetTickCount64()) + "_" + std::to_string(seq.fetch_add(1)));
    std::filesystem::create_directories(dir, ec);
    return dir;
}

void WriteRaw(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

std::string ReadAll(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

const char* kAcct = "01234567-89ab-cdef-0123-456789abcdef";
const char* kAcctB = "fedcba98-7654-3210-fedc-ba9876543210";

QuestHistoryEvent MakeHist(
    uint32_t qid,
    HistoryEventType type,
    ProgressState state,
    const char* at,
    const char* key,
    EvidenceKind ev = EvidenceKind::None)
{
    QuestHistoryEvent e;
    e.game_quest_id = qid;
    e.event_type = type;
    e.state = state;
    e.source = ProgressSource::GameSnapshot;
    e.confidence = Confidence::Confirmed;
    e.observed_at = at;
    e.semantic_event_key = key;
    e.evidence_kind = ev;
    return e;
}

::QuestProgress::QuestProgress MakeQuest(uint32_t id, ProgressState state, const char* at, const char* hist_key)
{
    ::QuestProgress::QuestProgress q;
    q.game_quest_id = id;
    q.state = state;
    q.source = ProgressSource::GameSnapshot;
    q.confidence = Confidence::Confirmed;
    q.first_observed_at = at;
    q.last_observed_at = at;
    q.history.push_back(MakeHist(id, HistoryEventType::Observation, state, at, hist_key));
    return q;
}

AccountProgressStore MakeStore(const char* account)
{
    AccountProgressStore s;
    s.store_format = kStoreFormatId;
    s.store_version = {kStoreFormatMajor, kStoreFormatMinor};
    s.account_key = account;
    StoredCharacter c;
    c.character_key = std::string(account) + "/aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    c.display_name = "Hero";
    c.profession = "W";
    c.is_pre_searing = false;
    c.first_observed_at = "2026-07-25T20:00:00.000Z";
    c.last_observed_at = "2026-07-25T20:00:00.000Z";
    c.quests.emplace(100, MakeQuest(100, ProgressState::Active, "2026-07-25T20:00:00.000Z", "key-a"));
    s.characters.emplace(c.character_key, c);
    return s;
}

void TestCodecRoundTripDeterministic()
{
    auto store = MakeStore(kAcct);
    ObjectiveObservation obj;
    obj.index = 0;
    obj.completed = false;
    obj.encoded_content = u"Kill rats";
    obj.content_fingerprint = FingerprintEncodedContent(obj.encoded_content);
    store.characters.begin()->second.quests.at(100).objectives = {obj};

    const auto a = SerializeAccountStoreJson(store);
    const auto b = SerializeAccountStoreJson(store);
    Expect(a.status == CodecStatus::Ok && b.status == CodecStatus::Ok, "codec_serialize_ok");
    Expect(a.utf8_json == b.utf8_json, "codec_deterministic_bytes");

    const auto parsed = ParseAccountStoreJson(a.utf8_json, kAcct);
    Expect(parsed.status == CodecStatus::Ok, "codec_parse_ok");
    Expect(parsed.store.account_key == NormalizeAccountKey(kAcct), "codec_account_key");
    Expect(parsed.store.characters.size() == 1, "codec_one_character");
    Expect(parsed.store.characters.begin()->second.quests.at(100).history[0].semantic_event_key == "key-a",
        "codec_semantic_key_preserved");
    Expect(parsed.store.characters.begin()->second.quests.at(100).objectives[0].encoded_content == u"Kill rats",
        "codec_utf16_roundtrip");
}

void TestCodecJourneyRoundTrip()
{
    auto store = MakeStore(kAcct);
    auto& character = store.characters.begin()->second;
    TitleStateRecord title;
    title.title_id = 12;
    title.tier_index = 3;
    title.current_points = 4500;
    title.last_observed_at = "2026-07-25T20:00:00.000Z";
    character.titles.emplace(12, title);
    character.last_known_level = 5;
    character.last_map_id = 73;
    character.secondary_profession = "5";
    character.is_pvp = true;
    character.experience_total = 999;
    character.skill_points_earned = 42;
    FactionTotalsRecord factions;
    factions.kurzick = 1000;
    character.faction_totals = factions;
    HomSnapshotRecord hom;
    hom.hom_code = "homcode";
    hom.observed_at = "2026-07-25T19:59:00.000Z";
    hom.resilience_points = 1;
    hom.resilience_dedicated = {1, 0};
    character.hall_of_monuments = hom;
    JourneyEventRecord ev;
    ev.kind = "title_tier";
    ev.subject_key = "title:12";
    ev.observed_at = "2026-07-25T20:00:00.000Z";
    ev.title_id = 12;
    ev.tier_index = 3;
    character.journey_events.push_back(ev);
    JourneyEventRecord map_ev;
    map_ev.kind = "map_enter";
    map_ev.subject_key = "map:73";
    map_ev.observed_at = "2026-07-25T20:01:00.000Z";
    map_ev.map_id = 73;
    character.journey_events.push_back(map_ev);

    const auto ser = SerializeAccountStoreJson(store);
    Expect(ser.status == CodecStatus::Ok, "codec_journey_serialize_ok");
    const auto parsed = ParseAccountStoreJson(ser.utf8_json, kAcct);
    Expect(parsed.status == CodecStatus::Ok, "codec_journey_parse_ok");
    const auto& round = parsed.store.characters.begin()->second;
    Expect(round.titles.at(12).tier_index == 3, "codec_journey_title_tier");
    Expect(round.last_known_level == 5, "codec_journey_level");
    Expect(round.last_map_id == 73, "codec_journey_last_map");
    Expect(round.secondary_profession == "5", "codec_secondary_profession");
    Expect(round.is_pvp.has_value() && *round.is_pvp, "codec_is_pvp");
    Expect(round.experience_total.has_value() && *round.experience_total == 999, "codec_experience_total");
    Expect(round.skill_points_earned.has_value() && *round.skill_points_earned == 42,
        "codec_skill_points_earned");
    Expect(round.faction_totals.has_value() && round.faction_totals->kurzick == 1000,
        "codec_faction_totals");
    Expect(round.hall_of_monuments.has_value()
            && round.hall_of_monuments->hom_code == "homcode"
            && round.hall_of_monuments->resilience_points == 1
            && round.hall_of_monuments->resilience_dedicated.size() == 2
            && round.hall_of_monuments->resilience_dedicated[0] == 1,
        "codec_hall_of_monuments");
    Expect(round.journey_events.size() == 2, "codec_journey_events_count");
    Expect(round.journey_events[0].kind == "title_tier", "codec_journey_event_kind");
    Expect(round.journey_events[1].kind == "map_enter", "codec_map_enter_kind");
    Expect(round.journey_events[1].map_id == 73, "codec_map_enter_id");
}

void TestCodecOrderingIndependent()
{
    AccountProgressStore store;
    store.account_key = kAcct;
    store.store_format = kStoreFormatId;
    store.store_version = {kStoreFormatMajor, kStoreFormatMinor};

    StoredCharacter c2;
    c2.character_key = "b-key";
    c2.display_name = "B";
    c2.first_observed_at = "2026-07-25T20:00:00.000Z";
    c2.last_observed_at = "2026-07-25T20:00:00.000Z";
    c2.quests.emplace(200, MakeQuest(200, ProgressState::Active, "2026-07-25T20:00:00.000Z", "kb"));

    StoredCharacter c1;
    c1.character_key = "a-key";
    c1.display_name = "A";
    c1.first_observed_at = "2026-07-25T20:00:00.000Z";
    c1.last_observed_at = "2026-07-25T20:00:00.000Z";
    c1.quests.emplace(100, MakeQuest(100, ProgressState::Active, "2026-07-25T20:00:00.000Z", "ka"));

    store.characters.emplace(c2.character_key, c2);
    store.characters.emplace(c1.character_key, c1);
    const auto json1 = SerializeAccountStoreJson(store);

    AccountProgressStore store2 = store;
    store2.characters.clear();
    store2.characters.emplace(c1.character_key, c1);
    store2.characters.emplace(c2.character_key, c2);
    const auto json2 = SerializeAccountStoreJson(store2);
    Expect(json1.utf8_json == json2.utf8_json, "codec_order_independent");
}

void TestCodecEnumsAndValidation()
{
    auto store = MakeStore(kAcct);
    store.characters.begin()->second.quests.at(100).state = ProgressState::Available;
    auto ser = SerializeAccountStoreJson(store);
    auto parsed = ParseAccountStoreJson(ser.utf8_json, kAcct);
    Expect(parsed.status == CodecStatus::Ok, "codec_reserved_state_allowed_in_store");
    Expect(parsed.store.characters.begin()->second.quests.at(100).state == ProgressState::Available,
        "codec_available_roundtrip");

    const std::string bad_enum = R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 1, "minor": 1 },
  "accountKey": "01234567-89ab-cdef-0123-456789abcdef",
  "characters": [{
    "characterKey": "c",
    "displayName": "X",
    "firstObservedAt": "2026-07-25T20:00:00.000Z",
    "lastObservedAt": "2026-07-25T20:00:00.000Z",
    "quests": [{
      "gameQuestId": 1,
      "state": "not_a_real_state",
      "source": "game_snapshot",
      "confidence": "confirmed",
      "firstObservedAt": "2026-07-25T20:00:00.000Z",
      "lastObservedAt": "2026-07-25T20:00:00.000Z",
      "objectives": [],
      "history": []
    }],
    "missions": []
  }]
})";
    auto bad = ParseAccountStoreJson(bad_enum, kAcct);
    Expect(bad.status == CodecStatus::ValidationError, "codec_invalid_enum_rejected");

    const std::string missing = R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 1, "minor": 1 },
  "accountKey": "01234567-89ab-cdef-0123-456789abcdef",
  "characters": [{
    "characterKey": "",
    "displayName": "X",
    "quests": [],
    "missions": []
  }]
})";
    auto miss = ParseAccountStoreJson(missing, kAcct);
    Expect(miss.status == CodecStatus::ValidationError, "codec_missing_character_key_rejected");

    const std::string unknown_opt = R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 1, "minor": 1 },
  "accountKey": "01234567-89ab-cdef-0123-456789abcdef",
  "futureOptional": true,
  "characters": []
})";
    auto unk = ParseAccountStoreJson(unknown_opt, kAcct);
    Expect(unk.status == CodecStatus::Ok, "codec_unknown_optional_tolerated");

    auto mismatch = ParseAccountStoreJson(unknown_opt, kAcctB);
    Expect(mismatch.status == CodecStatus::AccountKeyMismatch, "codec_account_mismatch");

    const std::string bad_ts = R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 1, "minor": 1 },
  "accountKey": "01234567-89ab-cdef-0123-456789abcdef",
  "characters": [{
    "characterKey": "c",
    "displayName": "X",
    "firstObservedAt": "2026-07-25T20:00:00Z",
    "lastObservedAt": "2026-07-25T20:00:00.000Z",
    "quests": [],
    "missions": []
  }]
})";
    auto ts = ParseAccountStoreJson(bad_ts, kAcct);
    Expect(ts.status == CodecStatus::ValidationError, "codec_bad_timestamp_rejected");
}

void TestCodecVersioning()
{
    const std::string newer = R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 2, "minor": 0 },
  "accountKey": "01234567-89ab-cdef-0123-456789abcdef",
  "characters": []
})";
    auto n = ParseAccountStoreJson(newer, kAcct);
    Expect(n.status == CodecStatus::UnsupportedNewerMajor, "codec_newer_major_rejected");

    const std::string newer_minor = R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 1, "minor": 99 },
  "accountKey": "01234567-89ab-cdef-0123-456789abcdef",
  "characters": []
})";
    auto nm = ParseAccountStoreJson(newer_minor, kAcct);
    Expect(nm.status == CodecStatus::UnsupportedNewerMajor, "codec_newer_minor_rejected");

    const std::string older = R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 1, "minor": 0 },
  "accountKey": "01234567-89ab-cdef-0123-456789abcdef",
  "characters": []
})";
    auto o = ParseAccountStoreJson(older, kAcct);
    Expect(o.status == CodecStatus::Ok, "codec_older_minor_migrated_ok");
    Expect(o.diagnostics.migrated, "codec_older_minor_migrated_flag");
    Expect(o.store.store_version.minor == kStoreFormatMinor, "codec_older_minor_now_current");
    Expect(o.store.account_skill_baseline.state == JourneyBaselineSealState::Unset,
        "codec_1_0_account_baseline_unset");
}

void ExpectUnsetBaselines(const CharacterJourneyBaselines& baselines, const char* tag)
{
    Expect(baselines.maps.state == JourneyBaselineSealState::Unset, tag);
    Expect(baselines.maps.ids.empty(), tag);
    Expect(baselines.character_skills.state == JourneyBaselineSealState::Unset, tag);
    Expect(baselines.heroes.state == JourneyBaselineSealState::Unset, tag);
    Expect(baselines.professions.state == JourneyBaselineSealState::Unset, tag);
    Expect(baselines.vanquish_areas.state == JourneyBaselineSealState::Unset, tag);
    Expect(baselines.hard_mode.state == JourneyBaselineSealState::Unset, tag);
    Expect(!baselines.hard_mode.unlocked, tag);
    Expect(baselines.skill_points.state == JourneyBaselineSealState::Unset, tag);
    Expect(baselines.factions.state == JourneyBaselineSealState::Unset, tag);
    Expect(baselines.hall_of_monuments.state == JourneyBaselineSealState::Unset, tag);
    Expect(baselines.cartography.state == JourneyBaselineSealState::Unset, tag);
    Expect(baselines.cartography.percent == 0, tag);
}

void TestJourneyBaselineCodecAndMerge()
{
    {
        auto store = MakeStore(kAcct);
        const auto ser = SerializeAccountStoreJson(store);
        Expect(ser.status == CodecStatus::Ok, "baseline_empty_serialize");
        const auto parsed = ParseAccountStoreJson(ser.utf8_json, kAcct);
        Expect(parsed.status == CodecStatus::Ok, "baseline_empty_parse");
        Expect(parsed.store.store_version.minor == 2, "baseline_empty_minor");
        ExpectUnsetBaselines(parsed.store.characters.begin()->second.journey_baselines,
            "baseline_empty_character");
        Expect(parsed.store.account_skill_baseline.state == JourneyBaselineSealState::Unset,
            "baseline_empty_account");
        const auto ser2 = SerializeAccountStoreJson(parsed.store);
        Expect(ser.utf8_json == ser2.utf8_json, "baseline_empty_deterministic");
    }

    {
        auto store = MakeStore(kAcct);
        auto& character = store.characters.begin()->second;
        character.journey_baselines.maps.state = JourneyBaselineSealState::Sealed;
        character.journey_baselines.maps.ids = {};
        const auto ser = SerializeAccountStoreJson(store);
        Expect(ser.status == CodecStatus::Ok, "baseline_sealed_empty_serialize");
        Expect(ser.utf8_json.find("\"state\": \"sealed\"") != std::string::npos,
            "baseline_sealed_empty_json_state");
        Expect(ser.utf8_json.find("\"ids\": [\n      ]") != std::string::npos
                || ser.utf8_json.find("\"ids\": []") != std::string::npos,
            "baseline_sealed_empty_json_ids");
        const auto parsed = ParseAccountStoreJson(ser.utf8_json, kAcct);
        Expect(parsed.status == CodecStatus::Ok, "baseline_sealed_empty_parse");
        Expect(parsed.store.characters.begin()->second.journey_baselines.maps.state
                == JourneyBaselineSealState::Sealed,
            "baseline_sealed_empty_state");
        Expect(parsed.store.characters.begin()->second.journey_baselines.maps.ids.empty(),
            "baseline_sealed_empty_ids");
    }

    {
        auto store = MakeStore(kAcct);
        auto& character = store.characters.begin()->second;
        character.journey_baselines.maps = {JourneyBaselineSealState::Sealed, {30, 10, 10, 20}};
        character.journey_baselines.character_skills = {JourneyBaselineSealState::Sealed, {5, 1}};
        character.journey_baselines.heroes = {JourneyBaselineSealState::Sealed, {6}};
        character.journey_baselines.professions = {JourneyBaselineSealState::Sealed, {1, 2}};
        character.journey_baselines.vanquish_areas = {JourneyBaselineSealState::Sealed, {73}};
        character.journey_baselines.hard_mode = {JourneyBaselineSealState::Sealed, true};
        character.journey_baselines.skill_points = {JourneyBaselineSealState::Sealed};
        character.journey_baselines.factions = {JourneyBaselineSealState::Sealed};
        character.journey_baselines.hall_of_monuments = {JourneyBaselineSealState::Sealed};
        character.journey_baselines.cartography = {JourneyBaselineSealState::Sealed, 42};
        store.account_skill_baseline = {JourneyBaselineSealState::Sealed, {9, 7, 7}};

        const auto ser = SerializeAccountStoreJson(store);
        Expect(ser.status == CodecStatus::Ok, "baseline_populated_serialize");
        const auto parsed = ParseAccountStoreJson(ser.utf8_json, kAcct);
        Expect(parsed.status == CodecStatus::Ok, "baseline_populated_parse");
        const auto& baselines = parsed.store.characters.begin()->second.journey_baselines;
        Expect(baselines.maps.ids == std::vector<uint32_t>({10, 20, 30}), "baseline_maps_sorted_dedup");
        Expect(baselines.character_skills.ids == std::vector<uint32_t>({1, 5}),
            "baseline_skills_sorted");
        Expect(baselines.heroes.ids == std::vector<uint32_t>({6}), "baseline_heroes");
        Expect(baselines.professions.ids == std::vector<uint32_t>({1, 2}), "baseline_professions");
        Expect(baselines.vanquish_areas.ids == std::vector<uint32_t>({73}), "baseline_vanquish");
        Expect(baselines.hard_mode.unlocked, "baseline_hard_mode_true");
        Expect(baselines.skill_points.state == JourneyBaselineSealState::Sealed, "baseline_sp");
        Expect(baselines.factions.state == JourneyBaselineSealState::Sealed, "baseline_faction");
        Expect(baselines.hall_of_monuments.state == JourneyBaselineSealState::Sealed, "baseline_hom");
        Expect(baselines.cartography.percent == 42, "baseline_cartography");
        Expect(parsed.store.account_skill_baseline.ids == std::vector<uint32_t>({7, 9}),
            "baseline_account_skills_sorted_dedup");
    }

    {
        auto store = MakeStore(kAcct);
        store.characters.begin()->second.journey_baselines.hard_mode = {
            JourneyBaselineSealState::Sealed, false};
        const auto parsed = ParseAccountStoreJson(SerializeAccountStoreJson(store).utf8_json, kAcct);
        Expect(parsed.status == CodecStatus::Ok, "baseline_hard_mode_false_ok");
        Expect(parsed.store.characters.begin()->second.journey_baselines.hard_mode.state
                == JourneyBaselineSealState::Sealed,
            "baseline_hard_mode_false_sealed");
        Expect(!parsed.store.characters.begin()->second.journey_baselines.hard_mode.unlocked,
            "baseline_hard_mode_false_value");
    }

    {
        auto store = MakeStore(kAcct);
        store.characters.begin()->second.journey_baselines.cartography = {
            JourneyBaselineSealState::Sealed, 0};
        Expect(ParseAccountStoreJson(SerializeAccountStoreJson(store).utf8_json, kAcct).status
                == CodecStatus::Ok,
            "baseline_cartography_0");
        store.characters.begin()->second.journey_baselines.cartography.percent = 100;
        Expect(ParseAccountStoreJson(SerializeAccountStoreJson(store).utf8_json, kAcct).status
                == CodecStatus::Ok,
            "baseline_cartography_100");
    }

    {
        const std::string bad_percent = R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 1, "minor": 2 },
  "accountKey": "01234567-89ab-cdef-0123-456789abcdef",
  "characters": [{
    "characterKey": "c",
    "displayName": "X",
    "firstObservedAt": "2026-07-25T20:00:00.000Z",
    "lastObservedAt": "2026-07-25T20:00:00.000Z",
    "quests": [],
    "missions": [],
    "journeyBaselines": {
      "cartography": { "state": "sealed", "percent": 101 }
    }
  }]
})";
        Expect(ParseAccountStoreJson(bad_percent, kAcct).status == CodecStatus::ValidationError,
            "baseline_cartography_101_reject");
    }

    {
        const std::string bad_state = R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 1, "minor": 2 },
  "accountKey": "01234567-89ab-cdef-0123-456789abcdef",
  "characters": [{
    "characterKey": "c",
    "displayName": "X",
    "firstObservedAt": "2026-07-25T20:00:00.000Z",
    "lastObservedAt": "2026-07-25T20:00:00.000Z",
    "quests": [],
    "missions": [],
    "journeyBaselines": {
      "maps": { "state": "pending", "ids": [] }
    }
  }]
})";
        Expect(ParseAccountStoreJson(bad_state, kAcct).status == CodecStatus::ValidationError,
            "baseline_unknown_state_reject");
    }

    {
        const std::string unset_payload = R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 1, "minor": 2 },
  "accountKey": "01234567-89ab-cdef-0123-456789abcdef",
  "characters": [{
    "characterKey": "c",
    "displayName": "X",
    "firstObservedAt": "2026-07-25T20:00:00.000Z",
    "lastObservedAt": "2026-07-25T20:00:00.000Z",
    "quests": [],
    "missions": [],
    "journeyBaselines": {
      "maps": { "state": "unset", "ids": [1] }
    }
  }]
})";
        Expect(ParseAccountStoreJson(unset_payload, kAcct).status == CodecStatus::ValidationError,
            "baseline_unset_with_ids_reject");
    }

    {
        auto store = MakeStore(kAcct);
        auto& character = store.characters.begin()->second;
        character.display_name = "KeepMe";
        character.journey_events.push_back(
            JourneyEventRecord{"map_enter", "map:1", "2026-07-25T20:02:00.000Z", 0, 0, 0, 1});
        TitleStateRecord title;
        title.title_id = 12;
        title.tier_index = 1;
        title.current_points = 10;
        title.last_observed_at = "2026-07-25T20:00:00.000Z";
        character.titles.emplace(12, title);
        MissionRecord mission;
        mission.map_id = 73;
        mission.completed_normal = true;
        mission.last_observed_at = "2026-07-25T20:00:00.000Z";
        character.missions.emplace(73, mission);
        character.skill_points_earned = 12;
        auto ser = SerializeAccountStoreJson(store);
        auto rewritten = ser.utf8_json;
        const auto pos = rewritten.find("\"minor\": 2");
        Expect(pos != std::string::npos, "baseline_migrate_find_minor");
        rewritten.replace(pos, std::string("\"minor\": 2").size(), "\"minor\": 0");
        auto migrated = ParseAccountStoreJson(rewritten, kAcct);
        Expect(migrated.status == CodecStatus::Ok, "baseline_migrate_1_0_ok");
        Expect(migrated.diagnostics.migrated, "baseline_migrate_1_0_flag");
        Expect(migrated.store.store_version.minor == 2, "baseline_migrate_1_0_to_1_2");
        Expect(
            std::count_if(
                migrated.diagnostics.messages.begin(),
                migrated.diagnostics.messages.end(),
                [](const std::string& m) { return m.find("1.0 -> 1.1") != std::string::npos; })
                == 1,
            "baseline_migrate_diag_1_0");
        Expect(
            std::count_if(
                migrated.diagnostics.messages.begin(),
                migrated.diagnostics.messages.end(),
                [](const std::string& m) { return m.find("1.1 -> 1.2") != std::string::npos; })
                == 1,
            "baseline_migrate_diag_1_1");
        const auto& round = migrated.store.characters.begin()->second;
        Expect(round.display_name == "KeepMe", "baseline_migrate_keeps_name");
        Expect(round.quests.size() == 1, "baseline_migrate_keeps_quest");
        Expect(round.quests.at(100).history.size() == 1, "baseline_migrate_keeps_history");
        Expect(round.journey_events.size() == 1, "baseline_migrate_keeps_journey");
        Expect(round.titles.size() == 1, "baseline_migrate_keeps_title");
        Expect(round.missions.size() == 1, "baseline_migrate_keeps_mission");
        Expect(round.skill_points_earned == 12, "baseline_migrate_keeps_snapshot");
        ExpectUnsetBaselines(round.journey_baselines, "baseline_migrate_1_0_unset");
    }

    {
        auto store = MakeStore(kAcct);
        auto ser = SerializeAccountStoreJson(store);
        auto rewritten = ser.utf8_json;
        const auto pos = rewritten.find("\"minor\": 2");
        Expect(pos != std::string::npos, "baseline_migrate_1_1_find");
        rewritten.replace(pos, std::string("\"minor\": 2").size(), "\"minor\": 1");
        auto migrated = ParseAccountStoreJson(rewritten, kAcct);
        Expect(migrated.status == CodecStatus::Ok, "baseline_migrate_1_1_ok");
        Expect(migrated.diagnostics.migrated, "baseline_migrate_1_1_flag");
        Expect(migrated.store.store_version.minor == 2, "baseline_migrate_1_1_to_1_2");
        ExpectUnsetBaselines(
            migrated.store.characters.begin()->second.journey_baselines, "baseline_migrate_1_1_unset");
    }

    {
        const std::string unknown_opt = R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 1, "minor": 2 },
  "accountKey": "01234567-89ab-cdef-0123-456789abcdef",
  "futureBaselineField": true,
  "characters": []
})";
        Expect(ParseAccountStoreJson(unknown_opt, kAcct).status == CodecStatus::Ok,
            "baseline_unknown_field_tolerated");
    }

    {
        AccountProgressStore disk = MakeStore(kAcct);
        disk.characters.begin()->second.journey_baselines.maps = {
            JourneyBaselineSealState::Sealed, {1, 2}};
        disk.characters.begin()->second.journey_events.push_back(
            JourneyEventRecord{"map_enter", "map:1", "2026-07-25T20:02:00.000Z", 0, 0, 0, 1});
        disk.account_skill_baseline = {JourneyBaselineSealState::Sealed, {10}};

        AccountProgressStore memory = MakeStore(kAcct);
        memory.characters.begin()->second.journey_baselines.maps.state =
            JourneyBaselineSealState::Unset;
        memory.characters.begin()->second.journey_events.push_back(
            JourneyEventRecord{"map_enter", "map:2", "2026-07-25T20:03:00.000Z", 0, 0, 0, 2});

        auto merged = MergeAccountStores(disk, memory);
        Expect(merged.status == StoreOpStatus::Ok, "baseline_merge_sealed_unset_ok");
        const auto& character = merged.merged->characters.begin()->second;
        Expect(character.journey_baselines.maps.state == JourneyBaselineSealState::Sealed,
            "baseline_merge_sealed_sticky");
        Expect(character.journey_baselines.maps.ids == std::vector<uint32_t>({1, 2}),
            "baseline_merge_sealed_payload");
        Expect(character.journey_events.size() == 2, "baseline_merge_keeps_journey_events");
        Expect(merged.merged->account_skill_baseline.ids == std::vector<uint32_t>({10}),
            "baseline_merge_account_sticky");
    }

    {
        AccountProgressStore disk = MakeStore(kAcct);
        disk.characters.begin()->second.journey_baselines.maps = {
            JourneyBaselineSealState::Sealed, {1, 3}};
        disk.characters.begin()->second.journey_baselines.hard_mode = {
            JourneyBaselineSealState::Sealed, false};
        disk.characters.begin()->second.journey_baselines.cartography = {
            JourneyBaselineSealState::Sealed, 20};
        disk.characters.begin()->second.journey_baselines.skill_points = {
            JourneyBaselineSealState::Sealed};
        disk.account_skill_baseline = {JourneyBaselineSealState::Sealed, {1, 4}};

        AccountProgressStore memory = MakeStore(kAcct);
        memory.characters.begin()->second.journey_baselines.maps = {
            JourneyBaselineSealState::Sealed, {2, 3}};
        memory.characters.begin()->second.journey_baselines.hard_mode = {
            JourneyBaselineSealState::Sealed, true};
        memory.characters.begin()->second.journey_baselines.cartography = {
            JourneyBaselineSealState::Sealed, 55};
        memory.characters.begin()->second.journey_baselines.skill_points = {
            JourneyBaselineSealState::Unset};
        memory.account_skill_baseline = {JourneyBaselineSealState::Sealed, {4, 8}};

        auto merged = MergeAccountStores(disk, memory);
        Expect(merged.status == StoreOpStatus::Ok, "baseline_merge_union_ok");
        const auto& character = merged.merged->characters.begin()->second;
        Expect(character.journey_baselines.maps.ids == std::vector<uint32_t>({1, 2, 3}),
            "baseline_merge_id_union");
        Expect(character.journey_baselines.hard_mode.unlocked, "baseline_merge_hard_mode_or");
        Expect(character.journey_baselines.cartography.percent == 55, "baseline_merge_cartography_max");
        Expect(character.journey_baselines.skill_points.state == JourneyBaselineSealState::Sealed,
            "baseline_merge_state_only_sticky");
        Expect(merged.merged->account_skill_baseline.ids == std::vector<uint32_t>({1, 4, 8}),
            "baseline_merge_account_union");
        Expect(character.quests.size() == 1, "baseline_merge_keeps_quests");
    }
}

bool DiagContainsPath(const CodecDiagnostics& diagnostics, std::string_view needle)
{
    for (const auto& message : diagnostics.messages) {
        if (message.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void TestJourneyBaselineReviewFixes()
{
    {
        auto store = MakeStore(kAcct);
        store.characters.begin()->second.journey_baselines.cartography = {
            JourneyBaselineSealState::Sealed, 101};
        const auto ser = SerializeAccountStoreJson(store);
        Expect(ser.status == CodecStatus::ValidationError, "baseline_ser_cartography_101");
        Expect(ser.utf8_json.empty(), "baseline_ser_cartography_101_no_json");
        Expect(DiagContainsPath(ser.diagnostics, "journeyBaselines.cartography"),
            "baseline_ser_cartography_101_path");
    }

    {
        auto store = MakeStore(kAcct);
        store.characters.begin()->second.journey_baselines.maps.state =
            static_cast<JourneyBaselineSealState>(99);
        const auto ser = SerializeAccountStoreJson(store);
        Expect(ser.status == CodecStatus::ValidationError, "baseline_ser_bad_char_enum");
        Expect(ser.utf8_json.empty(), "baseline_ser_bad_char_enum_no_json");
        Expect(DiagContainsPath(ser.diagnostics, "journeyBaselines.maps"),
            "baseline_ser_bad_char_enum_path");
    }

    {
        auto store = MakeStore(kAcct);
        store.account_skill_baseline.state = static_cast<JourneyBaselineSealState>(7);
        const auto ser = SerializeAccountStoreJson(store);
        Expect(ser.status == CodecStatus::ValidationError, "baseline_ser_bad_account_enum");
        Expect(ser.utf8_json.empty(), "baseline_ser_bad_account_enum_no_json");
        Expect(DiagContainsPath(ser.diagnostics, "accountSkillBaseline"),
            "baseline_ser_bad_account_enum_path");
    }

    {
        auto store = MakeStore(kAcct);
        store.characters.begin()->second.journey_baselines.maps = {
            JourneyBaselineSealState::Unset, {1}};
        Expect(SerializeAccountStoreJson(store).status == CodecStatus::ValidationError,
            "baseline_ser_unset_ids_reject");
        store = MakeStore(kAcct);
        store.characters.begin()->second.journey_baselines.hard_mode = {
            JourneyBaselineSealState::Unset, true};
        Expect(SerializeAccountStoreJson(store).status == CodecStatus::ValidationError,
            "baseline_ser_unset_unlocked_reject");
        store = MakeStore(kAcct);
        store.characters.begin()->second.journey_baselines.cartography = {
            JourneyBaselineSealState::Unset, 12};
        Expect(SerializeAccountStoreJson(store).status == CodecStatus::ValidationError,
            "baseline_ser_unset_percent_reject");
    }

    {
        const std::string missing_ids = R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 1, "minor": 2 },
  "accountKey": "01234567-89ab-cdef-0123-456789abcdef",
  "characters": [{
    "characterKey": "c",
    "displayName": "X",
    "firstObservedAt": "2026-07-25T20:00:00.000Z",
    "lastObservedAt": "2026-07-25T20:00:00.000Z",
    "quests": [],
    "missions": [],
    "journeyBaselines": {
      "maps": { "state": "sealed" }
    }
  }]
})";
        auto missing = ParseAccountStoreJson(missing_ids, kAcct);
        Expect(missing.status == CodecStatus::ValidationError, "baseline_parse_sealed_missing_ids");
        Expect(DiagContainsPath(missing.diagnostics, "journeyBaselines.maps"),
            "baseline_parse_sealed_missing_ids_path");
    }

    {
        const std::string empty_ids = R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 1, "minor": 2 },
  "accountKey": "01234567-89ab-cdef-0123-456789abcdef",
  "characters": [{
    "characterKey": "c",
    "displayName": "X",
    "firstObservedAt": "2026-07-25T20:00:00.000Z",
    "lastObservedAt": "2026-07-25T20:00:00.000Z",
    "quests": [],
    "missions": [],
    "journeyBaselines": {
      "maps": { "state": "sealed", "ids": [] }
    }
  }]
})";
        auto empty = ParseAccountStoreJson(empty_ids, kAcct);
        Expect(empty.status == CodecStatus::Ok, "baseline_parse_sealed_empty_ids");
        Expect(empty.store.characters.begin()->second.journey_baselines.maps.state
                == JourneyBaselineSealState::Sealed,
            "baseline_parse_sealed_empty_ids_state");
        Expect(empty.store.characters.begin()->second.journey_baselines.maps.ids.empty(),
            "baseline_parse_sealed_empty_ids_payload");
    }

    {
        auto store = MakeStore(kAcct);
        auto& character = store.characters.begin()->second;
        character.journey_baselines.maps = {JourneyBaselineSealState::Sealed, {3, 1, 2}};
        character.journey_baselines.hard_mode = {JourneyBaselineSealState::Sealed, false};
        character.journey_baselines.cartography = {JourneyBaselineSealState::Sealed, 100};
        store.account_skill_baseline = {JourneyBaselineSealState::Sealed, {9, 4}};
        const auto ser = SerializeAccountStoreJson(store);
        Expect(ser.status == CodecStatus::Ok, "baseline_valid_ser_ok");
        const auto parsed = ParseAccountStoreJson(ser.utf8_json, kAcct);
        Expect(parsed.status == CodecStatus::Ok, "baseline_valid_ser_roundtrip");
        Expect(parsed.store.characters.begin()->second.journey_baselines.maps.ids
                == std::vector<uint32_t>({1, 2, 3}),
            "baseline_valid_ser_maps");
        Expect(!parsed.store.characters.begin()->second.journey_baselines.hard_mode.unlocked,
            "baseline_valid_ser_hard_mode");
        Expect(parsed.store.characters.begin()->second.journey_baselines.cartography.percent == 100,
            "baseline_valid_ser_cartography");
        Expect(parsed.store.account_skill_baseline.ids == std::vector<uint32_t>({4, 9}),
            "baseline_valid_ser_account");
    }

    {
        const auto dir = MakeTempDir();
        const auto paths = BuildAccountStorePaths(dir, kAcct);
        auto valid = MakeStore(kAcct);
        valid.characters.begin()->second.journey_baselines.maps = {
            JourneyBaselineSealState::Sealed, {11}};
        const auto valid_json = SerializeAccountStoreJson(valid).utf8_json;
        WriteRaw(paths.primary, valid_json);
        WriteRaw(paths.backup, valid_json);
        const auto primary_before = ReadAll(paths.primary);
        const auto bak_before = ReadAll(paths.backup);

        auto memory = MakeStore(kAcct);
        memory.characters.begin()->second.journey_baselines.cartography = {
            JourneyBaselineSealState::Sealed, 101};
        auto saved = SaveMergedAccountStore(dir, kAcct, memory, 2000);
        Expect(saved.status == StoreOpStatus::CodecError, "baseline_save_invalid_percent_status");
        Expect(saved.diagnostics.dirty_retained, "baseline_save_invalid_percent_dirty");
        Expect(!saved.merged.has_value(), "baseline_save_invalid_percent_no_merged");
        Expect(ReadAll(paths.primary) == primary_before, "baseline_save_invalid_percent_primary");
        Expect(ReadAll(paths.backup) == bak_before, "baseline_save_invalid_percent_bak");
    }

    {
        const std::string literal_1_0 = R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 1, "minor": 0 },
  "accountKey": "01234567-89ab-cdef-0123-456789abcdef",
  "characters": [{
    "characterKey": "01234567-89ab-cdef-0123-456789abcdef/aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",
    "displayName": "LegacyHero",
    "profession": "W",
    "firstObservedAt": "2026-07-25T20:00:00.000Z",
    "lastObservedAt": "2026-07-25T20:05:00.000Z",
    "quests": [{
      "gameQuestId": 100,
      "state": "active",
      "source": "game_snapshot",
      "confidence": "confirmed",
      "firstObservedAt": "2026-07-25T20:00:00.000Z",
      "lastObservedAt": "2026-07-25T20:00:00.000Z",
      "objectives": [],
      "history": [{
        "gameQuestId": 100,
        "eventType": "observation",
        "state": "active",
        "source": "game_snapshot",
        "confidence": "confirmed",
        "observedAt": "2026-07-25T20:00:00.000Z",
        "semanticEventKey": "legacy-key",
        "objectives": [],
        "evidenceKind": "none"
      }]
    }],
    "missions": [{
      "mapId": 73,
      "completedNormal": true,
      "completedHard": false,
      "bonusNormal": false,
      "bonusHard": false,
      "lastObservedAt": "2026-07-25T20:01:00.000Z"
    }],
    "titles": [{
      "titleId": 12,
      "tierIndex": 2,
      "currentPoints": 100,
      "lastObservedAt": "2026-07-25T20:02:00.000Z"
    }],
    "lastKnownLevel": 12,
    "skillPointsEarned": 8,
    "journeyEvents": [{
      "kind": "map_enter",
      "subjectKey": "map:73",
      "observedAt": "2026-07-25T20:03:00.000Z",
      "mapId": 73
    }]
  }]
})";
        auto migrated = ParseAccountStoreJson(literal_1_0, kAcct);
        Expect(migrated.status == CodecStatus::Ok, "baseline_literal_1_0_ok");
        Expect(migrated.diagnostics.migrated, "baseline_literal_1_0_migrated");
        Expect(migrated.store.store_version.minor == 2, "baseline_literal_1_0_minor");
        Expect(migrated.store.account_skill_baseline.state == JourneyBaselineSealState::Unset,
            "baseline_literal_1_0_account_unset");
        const auto& character = migrated.store.characters.begin()->second;
        ExpectUnsetBaselines(character.journey_baselines, "baseline_literal_1_0_unset");
        Expect(character.display_name == "LegacyHero", "baseline_literal_1_0_name");
        Expect(character.quests.at(100).history[0].semantic_event_key == "legacy-key",
            "baseline_literal_1_0_history");
        Expect(character.journey_events.size() == 1, "baseline_literal_1_0_journey");
        Expect(character.titles.at(12).tier_index == 2, "baseline_literal_1_0_title");
        Expect(character.missions.at(73).completed_normal, "baseline_literal_1_0_mission");
        Expect(character.skill_points_earned == 8, "baseline_literal_1_0_snapshot");
        Expect(literal_1_0.find("journeyBaselines") == std::string::npos,
            "baseline_literal_1_0_no_baseline_field");
    }

    {
        const std::string literal_1_1 = R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 1, "minor": 1 },
  "accountKey": "01234567-89ab-cdef-0123-456789abcdef",
  "characters": [{
    "characterKey": "char-1-1",
    "displayName": "OneOne",
    "firstObservedAt": "2026-07-25T20:00:00.000Z",
    "lastObservedAt": "2026-07-25T20:00:00.000Z",
    "quests": [{
      "gameQuestId": 42,
      "state": "active",
      "source": "game_snapshot",
      "confidence": "confirmed",
      "firstObservedAt": "2026-07-25T20:00:00.000Z",
      "lastObservedAt": "2026-07-25T20:00:00.000Z",
      "objectives": [],
      "history": [{
        "gameQuestId": 42,
        "eventType": "observation",
        "state": "active",
        "source": "game_snapshot",
        "confidence": "confirmed",
        "observedAt": "2026-07-25T20:00:00.000Z",
        "semanticEventKey": "one-one-key",
        "objectives": [],
        "evidenceKind": "none"
      }]
    }],
    "missions": [],
    "journeyEvents": [{
      "kind": "level_up",
      "subjectKey": "level:5",
      "observedAt": "2026-07-25T20:04:00.000Z",
      "level": 5
    }]
  }]
})";
        auto migrated = ParseAccountStoreJson(literal_1_1, kAcct);
        Expect(migrated.status == CodecStatus::Ok, "baseline_literal_1_1_ok");
        Expect(migrated.diagnostics.migrated, "baseline_literal_1_1_migrated");
        Expect(migrated.store.store_version.minor == 2, "baseline_literal_1_1_minor");
        ExpectUnsetBaselines(
            migrated.store.characters.begin()->second.journey_baselines, "baseline_literal_1_1_unset");
        Expect(migrated.store.characters.begin()->second.quests.at(42).history[0].semantic_event_key
                == "one-one-key",
            "baseline_literal_1_1_history");
        Expect(migrated.store.characters.begin()->second.journey_events[0].kind == "level_up",
            "baseline_literal_1_1_journey");
        Expect(literal_1_1.find("journeyBaselines") == std::string::npos,
            "baseline_literal_1_1_no_baseline_field");
    }

    {
        const auto dir = MakeTempDir();
        const auto paths = BuildAccountStorePaths(dir, kAcct);
        const std::string newer_minor = R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 1, "minor": 99 },
  "accountKey": "01234567-89ab-cdef-0123-456789abcdef",
  "characters": []
})";
        const auto older_bak = SerializeAccountStoreJson(MakeStore(kAcct)).utf8_json;
        WriteRaw(paths.primary, newer_minor);
        WriteRaw(paths.backup, older_bak);
        const auto primary_before = ReadAll(paths.primary);
        const auto bak_before = ReadAll(paths.backup);
        auto loaded = LoadAccountStore(dir, kAcct);
        Expect(loaded.status == StoreOpStatus::UnsupportedDiskMajor, "baseline_newer_minor_no_downgrade");
        auto saved = SaveMergedAccountStore(dir, kAcct, MakeStore(kAcct), 2000);
        Expect(saved.status == StoreOpStatus::UnsupportedDiskMajor, "baseline_newer_minor_save_blocked");
        Expect(ReadAll(paths.primary) == primary_before, "baseline_newer_minor_primary_preserved");
        Expect(ReadAll(paths.backup) == bak_before, "baseline_newer_minor_bak_preserved");
    }

    {
        const auto dir = MakeTempDir();
        auto store = MakeStore(kAcct);
        auto& character = store.characters.begin()->second;
        character.journey_baselines.maps = {JourneyBaselineSealState::Sealed, {5, 8}};
        character.journey_baselines.hard_mode = {JourneyBaselineSealState::Sealed, true};
        character.journey_baselines.cartography = {JourneyBaselineSealState::Sealed, 33};
        store.account_skill_baseline = {JourneyBaselineSealState::Sealed, {100, 200}};
        auto saved = SaveMergedAccountStore(dir, kAcct, store, 2000);
        Expect(saved.status == StoreOpStatus::Ok, "baseline_e2e_save_ok");
        Expect(saved.merged.has_value(), "baseline_e2e_merged");
        auto loaded = LoadAccountStore(dir, kAcct);
        Expect(loaded.status == StoreOpStatus::Ok, "baseline_e2e_load_ok");
        Expect(loaded.store.characters.begin()->second.journey_baselines.maps.ids
                == std::vector<uint32_t>({5, 8}),
            "baseline_e2e_maps");
        Expect(loaded.store.characters.begin()->second.journey_baselines.hard_mode.unlocked,
            "baseline_e2e_hard_mode");
        Expect(loaded.store.characters.begin()->second.journey_baselines.cartography.percent == 33,
            "baseline_e2e_cartography");
        Expect(loaded.store.account_skill_baseline.ids == std::vector<uint32_t>({100, 200}),
            "baseline_e2e_account");
    }
}

void TestAtomicCreateReplaceBak()
{
    const auto dir = MakeTempDir();
    const auto paths = BuildAccountStorePaths(dir, kAcct);
    auto store = MakeStore(kAcct);
    auto ser = SerializeAccountStoreJson(store);
    Expect(ser.status == CodecStatus::Ok, "atomic_ser_ok");

    auto first = AtomicJson::WriteAtomicUtf8(paths.primary, paths.backup, ser.utf8_json);
    Expect(first.ok && first.used_move_file_ex, "atomic_first_create_movefileex");
    Expect(std::filesystem::exists(paths.primary), "atomic_primary_exists");
    Expect(!std::filesystem::exists(paths.backup), "atomic_no_bak_on_first_create");

    store.characters.begin()->second.display_name = "Hero2";
    auto ser2 = SerializeAccountStoreJson(store);
    auto second = AtomicJson::WriteAtomicUtf8(paths.primary, paths.backup, ser2.utf8_json);
    Expect(second.ok && second.used_replace_file, "atomic_replace_file");
    Expect(std::filesystem::exists(paths.backup), "atomic_bak_created");

    WriteRaw(paths.primary, "{not-json");
    auto loaded = LoadAccountStore(dir, kAcct);
    Expect(loaded.status == StoreOpStatus::RecoveredFromBak, "load_recover_bak");
    Expect(loaded.store.characters.begin()->second.display_name == "Hero", "load_bak_content");
    Expect(std::filesystem::exists(paths.primary), "load_primary_preserved");
    Expect(std::filesystem::exists(paths.backup), "load_bak_preserved");

    WriteRaw(paths.primary, "{bad");
    WriteRaw(paths.backup, "{also-bad");
    auto both_bad = LoadAccountStore(dir, kAcct);
    Expect(both_bad.status == StoreOpStatus::CodecError, "load_both_bad_fails");
    Expect(std::filesystem::exists(paths.primary) && std::filesystem::exists(paths.backup),
        "load_both_bad_files_kept");

    std::error_code ec;
    std::filesystem::remove(paths.primary, ec);
    std::filesystem::remove(paths.backup, ec);
    auto missing = LoadAccountStore(dir, kAcct);
    Expect(missing.status == StoreOpStatus::Empty, "load_missing_empty");
    Expect(!std::filesystem::exists(paths.primary), "load_missing_no_create");

    auto recreate = AtomicJson::WriteAtomicUtf8(paths.primary, paths.backup, ser.utf8_json);
    Expect(recreate.ok, "atomic_recreate_ok");
    WriteRaw(paths.tmp, "{stale-tmp");
    auto still = LoadAccountStore(dir, kAcct);
    Expect(still.status == StoreOpStatus::Ok, "stale_tmp_ignored_for_load");
    Expect(still.store.account_key == NormalizeAccountKey(kAcct), "stale_tmp_not_primary");
}

void TestLoadSaveStatusMatrix()
{
    const auto dir = MakeTempDir();
    const auto paths = BuildAccountStorePaths(dir, kAcct);
    auto memory = MakeStore(kAcct);
    const auto valid_json = SerializeAccountStoreJson(MakeStore(kAcct)).utf8_json;

    // A: missing primary + missing backup → Empty
    auto missing = LoadAccountStore(dir, kAcct);
    Expect(missing.status == StoreOpStatus::Empty, "matrix_missing_empty");
    Expect(missing.store.characters.empty(), "matrix_missing_empty_store");

    // B: zero-byte primary + no backup → CodecError; save blocked; bytes unchanged
    WriteRaw(paths.primary, "");
    Expect(std::filesystem::file_size(paths.primary) == 0, "matrix_zero_byte_exists");
    auto zero = LoadAccountStore(dir, kAcct);
    Expect(zero.status == StoreOpStatus::CodecError, "matrix_zero_codec_error");
    const auto zero_bytes = ReadAll(paths.primary);
    auto save_zero = SaveMergedAccountStore(dir, kAcct, memory, 2000);
    Expect(save_zero.status == StoreOpStatus::CodecError, "matrix_zero_save_blocked");
    Expect(!save_zero.merged.has_value(), "matrix_zero_save_no_merged");
    Expect(ReadAll(paths.primary) == zero_bytes, "matrix_zero_bytes_preserved");
    Expect(!std::filesystem::exists(paths.backup), "matrix_zero_no_bak_created");

    // whitespace-only primary + no backup → CodecError
    WriteRaw(paths.primary, " \n\t  ");
    const auto ws_bytes = ReadAll(paths.primary);
    auto ws = LoadAccountStore(dir, kAcct);
    Expect(ws.status == StoreOpStatus::CodecError, "matrix_whitespace_codec_error");
    auto save_ws = SaveMergedAccountStore(dir, kAcct, memory, 2000);
    Expect(save_ws.status == StoreOpStatus::CodecError, "matrix_whitespace_save_blocked");
    Expect(ReadAll(paths.primary) == ws_bytes, "matrix_whitespace_bytes_preserved");

    // malformed primary + no backup → CodecError
    WriteRaw(paths.primary, "{not-json");
    const auto bad_bytes = ReadAll(paths.primary);
    auto bad = LoadAccountStore(dir, kAcct);
    Expect(bad.status == StoreOpStatus::CodecError, "matrix_malformed_codec_error");
    auto save_bad = SaveMergedAccountStore(dir, kAcct, memory, 2000);
    Expect(save_bad.status == StoreOpStatus::CodecError, "matrix_malformed_save_blocked");
    Expect(ReadAll(paths.primary) == bad_bytes, "matrix_malformed_bytes_preserved");

    // zero-byte primary + valid backup → recovered
    WriteRaw(paths.primary, "");
    WriteRaw(paths.backup, valid_json);
    auto zero_bak = LoadAccountStore(dir, kAcct);
    Expect(zero_bak.status == StoreOpStatus::RecoveredFromBak, "matrix_zero_valid_bak");
    Expect(zero_bak.store.characters.size() == 1, "matrix_zero_valid_bak_store");

    // malformed primary + valid backup → recovered
    WriteRaw(paths.primary, "{broken");
    WriteRaw(paths.backup, valid_json);
    auto mal_bak = LoadAccountStore(dir, kAcct);
    Expect(mal_bak.status == StoreOpStatus::RecoveredFromBak, "matrix_malformed_valid_bak");
    Expect(mal_bak.store.characters.begin()->second.display_name == "Hero", "matrix_malformed_valid_bak_name");

    // unsupported-major primary + valid older backup → UnsupportedDiskMajor, no downgrade
    const std::string newer_major = R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 2, "minor": 0 },
  "accountKey": "01234567-89ab-cdef-0123-456789abcdef",
  "characters": []
})";
    WriteRaw(paths.primary, newer_major);
    WriteRaw(paths.backup, valid_json);
    const auto major_primary = ReadAll(paths.primary);
    const auto major_bak = ReadAll(paths.backup);
    auto major = LoadAccountStore(dir, kAcct);
    Expect(major.status == StoreOpStatus::UnsupportedDiskMajor, "matrix_newer_major_no_downgrade");
    auto save_major = SaveMergedAccountStore(dir, kAcct, memory, 2000);
    Expect(save_major.status == StoreOpStatus::UnsupportedDiskMajor, "matrix_newer_major_save_blocked");
    Expect(ReadAll(paths.primary) == major_primary, "matrix_newer_major_primary_preserved");
    Expect(ReadAll(paths.backup) == major_bak, "matrix_newer_major_bak_preserved");

    // account-key mismatch → blocked, no write
    const std::string other_acct = R"({
  "storeFormat": "gwtoolbox-quest-progress",
  "storeVersion": { "major": 1, "minor": 1 },
  "accountKey": "fedcba98-7654-3210-fedc-ba9876543210",
  "characters": []
})";
    WriteRaw(paths.primary, other_acct);
    std::error_code ec;
    std::filesystem::remove(paths.backup, ec);
    const auto mismatch_bytes = ReadAll(paths.primary);
    auto mismatch_load = LoadAccountStore(dir, kAcct);
    Expect(mismatch_load.status == StoreOpStatus::CodecError, "matrix_account_mismatch_load");
    auto mismatch_save = SaveMergedAccountStore(dir, kAcct, memory, 2000);
    Expect(mismatch_save.status == StoreOpStatus::CodecError, "matrix_account_mismatch_save_blocked");
    Expect(ReadAll(paths.primary) == mismatch_bytes, "matrix_account_mismatch_bytes_preserved");
}

void TestMergeHistoryCanonicalization()
{
    auto base = MakeStore(kAcct);
    const auto ck = base.characters.begin()->first;

    // Disk history unsorted: latest valid event is in the middle.
    auto disk = base;
    auto& dq = disk.characters.at(ck).quests.at(100);
    dq.history.clear();
    dq.history.push_back(MakeHist(100, HistoryEventType::Observation, ProgressState::Active,
        "2026-07-25T20:00:00.000Z", "key-early"));
    dq.history.push_back(MakeHist(100, HistoryEventType::Observation, ProgressState::ObjectiveProgress,
        "2026-07-25T22:00:00.000Z", "key-latest"));
    dq.history.push_back(MakeHist(100, HistoryEventType::Observation, ProgressState::ReadyForReward,
        "2026-07-25T21:00:00.000Z", "key-mid"));
    dq.last_observed_at = "2026-07-25T20:00:00.000Z";
    dq.state = ProgressState::Active;

    auto memory_old = base;
    memory_old.characters.at(ck).quests.at(100).state = ProgressState::Active;
    memory_old.characters.at(ck).quests.at(100).last_observed_at = "2026-07-25T19:00:00.000Z";
    memory_old.characters.at(ck).quests.at(100).history = {
        MakeHist(100, HistoryEventType::Observation, ProgressState::Active,
            "2026-07-25T19:00:00.000Z", "key-mem-old")};

    // Prefer disk because history contains newer valid event despite unsorted order / stale last_observed_at.
    auto from_disk = MergeAccountStores(disk, memory_old);
    Expect(from_disk.status == StoreOpStatus::Ok && from_disk.merged.has_value(), "hist_disk_unsorted_ok");
    // Effective recency on disk is 22:00 via history → disk projection (Active) wins over older memory.
    Expect(from_disk.merged->characters.at(ck).quests.at(100).state == ProgressState::Active,
        "hist_disk_unsorted_selects_latest_valid");

    // Memory history unsorted with latest buried; should win over older disk.
    auto disk_old = base;
    auto memory = base;
    auto& mq = memory.characters.at(ck).quests.at(100);
    mq.state = ProgressState::ObjectiveProgress;
    mq.last_observed_at = "2026-07-25T20:00:00.000Z";
    mq.history.clear();
    mq.history.push_back(MakeHist(100, HistoryEventType::Observation, ProgressState::Active,
        "2026-07-25T20:00:00.000Z", "key-a"));
    mq.history.push_back(MakeHist(100, HistoryEventType::Observation, ProgressState::ObjectiveProgress,
        "2026-07-25T23:00:00.000Z", "key-late"));
    mq.history.push_back(MakeHist(100, HistoryEventType::Observation, ProgressState::ReadyForReward,
        "2026-07-25T21:00:00.000Z", "key-mid"));

    auto from_mem = MergeAccountStores(disk_old, memory);
    Expect(from_mem.status == StoreOpStatus::Ok && from_mem.merged.has_value(), "hist_mem_unsorted_ok");
    Expect(from_mem.merged->characters.at(ck).quests.at(100).state == ProgressState::ObjectiveProgress,
        "hist_mem_unsorted_selects_latest_valid");

    // Differently ordered equivalent histories → same projection.
    auto mem_a = memory;
    auto mem_b = memory;
    std::reverse(mem_b.characters.at(ck).quests.at(100).history.begin(),
        mem_b.characters.at(ck).quests.at(100).history.end());
    auto m1 = MergeAccountStores(disk_old, mem_a);
    auto m2 = MergeAccountStores(disk_old, mem_b);
    Expect(m1.status == StoreOpStatus::Ok && m2.status == StoreOpStatus::Ok, "hist_order_independent_ok");
    Expect(m1.merged->characters.at(ck).quests.at(100).state
            == m2.merged->characters.at(ck).quests.at(100).state,
        "hist_order_independent_state");
    Expect(SerializeAccountStoreJson(*m1.merged).utf8_json == SerializeAccountStoreJson(*m2.merged).utf8_json,
        "hist_order_independent_bytes");

    // Equal timestamps: higher semanticEventKey wins.
    auto disk_tie = base;
    auto mem_tie = base;
    disk_tie.characters.at(ck).quests.at(100).state = ProgressState::Active;
    disk_tie.characters.at(ck).quests.at(100).last_observed_at = "2026-07-25T20:00:00.000Z";
    disk_tie.characters.at(ck).quests.at(100).history = {
        MakeHist(100, HistoryEventType::Observation, ProgressState::Active,
            "2026-07-25T20:00:00.000Z", "key-aaa")};
    mem_tie.characters.at(ck).quests.at(100).state = ProgressState::ObjectiveProgress;
    mem_tie.characters.at(ck).quests.at(100).last_observed_at = "2026-07-25T20:00:00.000Z";
    mem_tie.characters.at(ck).quests.at(100).history = {
        MakeHist(100, HistoryEventType::Observation, ProgressState::ObjectiveProgress,
            "2026-07-25T20:00:00.000Z", "key-zzz")};
    auto tied = MergeAccountStores(disk_tie, mem_tie);
    Expect(tied.status == StoreOpStatus::Ok && tied.merged.has_value(), "hist_tie_ok");
    Expect(tied.merged->characters.at(ck).quests.at(100).state == ProgressState::ObjectiveProgress,
        "hist_tie_semantic_key");

    // Malformed trailing event must not override a valid newer canonical event.
    auto disk_mal = base;
    auto mem_mal = base;
    disk_mal.characters.at(ck).quests.at(100).state = ProgressState::ObjectiveProgress;
    disk_mal.characters.at(ck).quests.at(100).last_observed_at = "2026-07-25T22:00:00.000Z";
    disk_mal.characters.at(ck).quests.at(100).history = {
        MakeHist(100, HistoryEventType::Observation, ProgressState::ObjectiveProgress,
            "2026-07-25T22:00:00.000Z", "key-good")};
    mem_mal.characters.at(ck).quests.at(100).state = ProgressState::ReadyForReward;
    mem_mal.characters.at(ck).quests.at(100).last_observed_at = "2026-07-25T21:00:00.000Z";
    mem_mal.characters.at(ck).quests.at(100).history = {
        MakeHist(100, HistoryEventType::Observation, ProgressState::Active,
            "2026-07-25T21:00:00.000Z", "key-older"),
        MakeHist(100, HistoryEventType::Observation, ProgressState::ReadyForReward,
            "not-a-timestamp", "key-malformed-trailing")};
    auto mal = MergeAccountStores(disk_mal, mem_mal);
    Expect(mal.status == StoreOpStatus::Ok && mal.merged.has_value(), "hist_malformed_ok");
    Expect(mal.merged->characters.at(ck).quests.at(100).state == ProgressState::ObjectiveProgress,
        "hist_malformed_trailing_ignored");
}

void TestMergeConflictUnusable()
{
    const auto dir = MakeTempDir();
    const auto paths = BuildAccountStorePaths(dir, kAcct);
    auto disk = MakeStore(kAcct);
    auto ser = SerializeAccountStoreJson(disk);
    Expect(AtomicJson::WriteAtomicUtf8(paths.primary, paths.backup, ser.utf8_json).ok, "conflict_seed_write");
    // Create a bak with known bytes via second write.
    disk.characters.begin()->second.display_name = "HeroBak";
    auto ser2 = SerializeAccountStoreJson(disk);
    Expect(AtomicJson::WriteAtomicUtf8(paths.primary, paths.backup, ser2.utf8_json).ok, "conflict_seed_bak");
    // Restore primary content for conflict scenario: write original Hero store as primary again.
    disk.characters.begin()->second.display_name = "Hero";
    auto ser_primary = SerializeAccountStoreJson(disk);
    WriteRaw(paths.primary, ser_primary.utf8_json);

    const auto primary_before = ReadAll(paths.primary);
    const auto bak_before = ReadAll(paths.backup);

    auto conflict_mem = disk;
    conflict_mem.characters.begin()->second.quests.at(100).history[0].state = ProgressState::ReadyForReward;
    auto conflict = MergeAccountStores(disk, conflict_mem);
    Expect(conflict.status == StoreOpStatus::MergeConflict, "merge_conflict_same_key");
    Expect(!conflict.merged.has_value(), "merge_conflict_no_usable_store");

    auto saved = SaveMergedAccountStore(dir, kAcct, conflict_mem, 2000);
    Expect(saved.status == StoreOpStatus::MergeConflict, "merge_conflict_save_blocked");
    Expect(!saved.merged.has_value(), "merge_conflict_save_no_merged");
    Expect(ReadAll(paths.primary) == primary_before, "merge_conflict_primary_unchanged");
    Expect(ReadAll(paths.backup) == bak_before, "merge_conflict_bak_unchanged");
}

void TestMergeOnWrite()
{
    const auto dir = MakeTempDir();
    auto disk = MakeStore(kAcct);
    StoredCharacter other;
    other.character_key = std::string(kAcct) + "/bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb";
    other.display_name = "Other";
    other.first_observed_at = "2026-07-25T20:00:00.000Z";
    other.last_observed_at = "2026-07-25T20:00:00.000Z";
    disk.characters.emplace(other.character_key, other);

    auto memory = MakeStore(kAcct);
    memory.characters.begin()->second.display_name = "HeroRenamed";
    memory.characters.begin()->second.last_observed_at = "2026-07-25T21:00:00.000Z";
    auto& q = memory.characters.begin()->second.quests.at(100);
    q.last_observed_at = "2026-07-25T21:00:00.000Z";
    q.history.push_back(MakeHist(100, HistoryEventType::Observation, ProgressState::ObjectiveProgress,
        "2026-07-25T21:00:00.000Z", "key-b"));
    q.state = ProgressState::ObjectiveProgress;

    auto merged = MergeAccountStores(disk, memory);
    Expect(merged.status == StoreOpStatus::Ok && merged.merged.has_value(), "merge_ok");
    Expect(merged.merged->characters.size() == 2, "merge_keeps_separate_characters");
    Expect(merged.merged->characters.at(memory.characters.begin()->first).display_name == "HeroRenamed",
        "merge_newer_display_name");
    const auto& mq = merged.merged->characters.at(memory.characters.begin()->first).quests.at(100);
    Expect(mq.history.size() == 2, "merge_history_union");
    Expect(mq.state == ProgressState::ObjectiveProgress, "merge_newer_projection");

    auto older = memory;
    older.characters.begin()->second.last_observed_at = "2026-07-25T19:00:00.000Z";
    older.characters.begin()->second.quests.at(100).last_observed_at = "2026-07-25T19:00:00.000Z";
    older.characters.begin()->second.quests.at(100).history = {
        MakeHist(100, HistoryEventType::Observation, ProgressState::Active,
            "2026-07-25T19:00:00.000Z", "key-old")};
    older.characters.begin()->second.quests.at(100).state = ProgressState::Active;
    auto no_rollback = MergeAccountStores(*merged.merged, older);
    Expect(no_rollback.status == StoreOpStatus::Ok && no_rollback.merged.has_value(), "merge_no_rollback_ok");
    Expect(no_rollback.merged->characters.at(memory.characters.begin()->first).quests.at(100).state
            == ProgressState::ObjectiveProgress,
        "merge_no_rollback");

    MissionRecord m1{10, true, false, false, false, "2026-07-25T20:00:00.000Z"};
    MissionRecord m2{10, false, true, false, false, "2026-07-25T22:00:00.000Z"};
    disk.characters.begin()->second.missions[10] = m1;
    memory.characters.begin()->second.missions[10] = m2;
    auto mm = MergeAccountStores(disk, memory);
    Expect(mm.status == StoreOpStatus::Ok && mm.merged.has_value(), "merge_mission_ok");
    Expect(mm.merged->characters.at(memory.characters.begin()->first).missions.at(10).completed_hard,
        "merge_mission_newer");

    Expect(merged.merged->characters.count(other.character_key) == 1, "merge_no_name_key_collapse");

    auto saved = SaveMergedAccountStore(dir, kAcct, memory, 2000);
    Expect(saved.status == StoreOpStatus::Ok && saved.merged.has_value(), "save_merged_ok");
}

void TestMissionEqualTimestampOr()
{
    auto disk = MakeStore(kAcct);
    auto memory = MakeStore(kAcct);
    const auto ck = disk.characters.begin()->first;
    const char* ts = "2026-07-25T20:00:00.000Z";

    MissionRecord a{10, true, false, true, false, ts};
    MissionRecord b{10, false, true, false, true, ts};
    disk.characters.at(ck).missions[10] = a;
    memory.characters.at(ck).missions[10] = b;

    auto ab = MergeAccountStores(disk, memory);
    auto ba = MergeAccountStores(memory, disk);
    Expect(ab.status == StoreOpStatus::Ok && ba.status == StoreOpStatus::Ok, "mission_or_ok");
    const auto& m_ab = ab.merged->characters.at(ck).missions.at(10);
    const auto& m_ba = ba.merged->characters.at(ck).missions.at(10);
    Expect(m_ab.completed_normal && m_ab.completed_hard && m_ab.bonus_normal && m_ab.bonus_hard,
        "mission_or_all_flags");
    Expect(m_ab.completed_normal == m_ba.completed_normal
            && m_ab.completed_hard == m_ba.completed_hard
            && m_ab.bonus_normal == m_ba.bonus_normal
            && m_ab.bonus_hard == m_ba.bonus_hard,
        "mission_or_order_independent");

    // Never clears a previously observed completion when merging equal timestamps.
    MissionRecord only_true{10, true, true, true, true, ts};
    MissionRecord only_false{10, false, false, false, false, ts};
    disk.characters.at(ck).missions[10] = only_true;
    memory.characters.at(ck).missions[10] = only_false;
    auto keep = MergeAccountStores(disk, memory);
    Expect(keep.merged->characters.at(ck).missions.at(10).completed_normal
            && keep.merged->characters.at(ck).missions.at(10).completed_hard
            && keep.merged->characters.at(ck).missions.at(10).bonus_normal
            && keep.merged->characters.at(ck).missions.at(10).bonus_hard,
        "mission_or_never_clears");
}

void TestMutexNamingAndWaitClassify()
{
    const auto a = BuildAccountMutexName(kAcct);
    const auto b = BuildAccountMutexName(kAcctB);
    const auto a2 = BuildAccountMutexName(kAcct);
    Expect(a == a2, "mutex_deterministic");
    Expect(a != b, "mutex_different_accounts");
    Expect(a.find(kAcct) == std::string::npos, "mutex_no_raw_account_key");
    Expect(a.rfind("Local\\GWToolbox.QuestProgress.", 0) == 0, "mutex_prefix");
    Expect(a.size() == std::string("Local\\GWToolbox.QuestProgress.").size() + 16, "mutex_fixed_hash_len");

    Expect(ClassifyWaitResult(WAIT_OBJECT_0) == WaitAcquireKind::Acquired, "wait_acquired");
    Expect(ClassifyWaitResult(WAIT_ABANDONED) == WaitAcquireKind::Abandoned, "wait_abandoned");
    Expect(ClassifyWaitResult(WAIT_TIMEOUT) == WaitAcquireKind::Timeout, "wait_timeout");
    Expect(ClassifyWaitResult(WAIT_FAILED) == WaitAcquireKind::Failed, "wait_failed");
}

void TestSaveTimeoutDoesNotWrite()
{
    const auto dir = MakeTempDir();
    const auto name = BuildAccountMutexName(kAcct);
    const std::wstring wide(name.begin(), name.end());

    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE held = nullptr;
    std::thread holder([&]() {
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
        WaitForSingleObject(release, INFINITE);
        ReleaseMutex(held);
        CloseHandle(held);
        held = nullptr;
    });

    WaitForSingleObject(ready, 5000);
    Expect(held != nullptr, "timeout_setup_mutex");

    auto memory = MakeStore(kAcct);
    auto saved = SaveMergedAccountStore(dir, kAcct, memory, 100);
    Expect(saved.status == StoreOpStatus::LockTimeout, "save_timeout");
    Expect(saved.diagnostics.dirty_retained, "save_timeout_dirty");
    Expect(!std::filesystem::exists(BuildAccountStorePaths(dir, kAcct).primary), "save_timeout_no_write");

    SetEvent(release);
    holder.join();
    if (ready) CloseHandle(ready);
    if (release) CloseHandle(release);
}

} // namespace

void RunBatch2BStoreTests()
{
    TestCodecRoundTripDeterministic();
    TestCodecJourneyRoundTrip();
    TestCodecOrderingIndependent();
    TestCodecEnumsAndValidation();
    TestCodecVersioning();
    TestJourneyBaselineCodecAndMerge();
    TestJourneyBaselineReviewFixes();
    TestAtomicCreateReplaceBak();
    TestLoadSaveStatusMatrix();
    TestMergeHistoryCanonicalization();
    TestMergeConflictUnusable();
    TestMergeOnWrite();
    TestMissionEqualTimestampOr();
    TestMutexNamingAndWaitClassify();
    TestSaveTimeoutDoesNotWrite();
}
