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
    JourneyEventRecord ev;
    ev.kind = "title_tier";
    ev.subject_key = "title:12";
    ev.observed_at = "2026-07-25T20:00:00.000Z";
    ev.title_id = 12;
    ev.tier_index = 3;
    character.journey_events.push_back(ev);

    const auto ser = SerializeAccountStoreJson(store);
    Expect(ser.status == CodecStatus::Ok, "codec_journey_serialize_ok");
    const auto parsed = ParseAccountStoreJson(ser.utf8_json, kAcct);
    Expect(parsed.status == CodecStatus::Ok, "codec_journey_parse_ok");
    const auto& round = parsed.store.characters.begin()->second;
    Expect(round.titles.at(12).tier_index == 3, "codec_journey_title_tier");
    Expect(round.last_known_level == 5, "codec_journey_level");
    Expect(round.journey_events.size() == 1, "codec_journey_events_count");
    Expect(round.journey_events[0].kind == "title_tier", "codec_journey_event_kind");
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
    TestAtomicCreateReplaceBak();
    TestLoadSaveStatusMatrix();
    TestMergeHistoryCanonicalization();
    TestMergeConflictUnusable();
    TestMergeOnWrite();
    TestMissionEqualTimestampOr();
    TestMutexNamingAndWaitClassify();
    TestSaveTimeoutDoesNotWrite();
}
