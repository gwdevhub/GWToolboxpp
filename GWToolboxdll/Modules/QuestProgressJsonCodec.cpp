#include <Modules/QuestProgressJsonCodec.h>

#include <glaze/glaze.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace QuestProgress {

struct JsonStoreVersion {
    uint32_t major = 0;
    uint32_t minor = 0;
};

struct JsonObjective {
    uint32_t index = 0;
    bool completed = false;
    std::string encoded_content_hex;
    std::string content_fingerprint;
};

struct JsonHistoryEvent {
    uint32_t game_quest_id = 0;
    std::string event_type;
    std::string state;
    std::string source;
    std::string confidence;
    std::string observed_at;
    std::string semantic_event_key;
    std::vector<JsonObjective> objectives;
    std::string evidence_kind;
};

struct JsonQuest {
    uint32_t game_quest_id = 0;
    std::string state;
    std::string source;
    std::string confidence;
    std::string first_observed_at;
    std::string last_observed_at;
    std::optional<std::string> completed_at;
    std::vector<JsonObjective> objectives;
    std::vector<JsonHistoryEvent> history;
};

struct JsonMission {
    uint32_t map_id = 0;
    bool completed_normal = false;
    bool completed_hard = false;
    bool bonus_normal = false;
    bool bonus_hard = false;
    std::string last_observed_at;
};

struct JsonCharacter {
    std::string character_key;
    std::string display_name;
    std::string profession;
    std::optional<bool> is_pre_searing;
    std::string first_observed_at;
    std::string last_observed_at;
    std::vector<JsonQuest> quests;
    std::vector<JsonMission> missions;
};

struct JsonAccountStore {
    std::string store_format;
    JsonStoreVersion store_version{};
    std::string account_key;
    std::vector<JsonCharacter> characters;
};

} // namespace QuestProgress

template <>
struct glz::meta<QuestProgress::JsonStoreVersion> {
    using T = QuestProgress::JsonStoreVersion;
    static constexpr auto value = object("major", &T::major, "minor", &T::minor);
};

template <>
struct glz::meta<QuestProgress::JsonObjective> {
    using T = QuestProgress::JsonObjective;
    static constexpr auto value = object(
        "index", &T::index,
        "completed", &T::completed,
        "encodedContentHex", &T::encoded_content_hex,
        "contentFingerprint", &T::content_fingerprint);
};

template <>
struct glz::meta<QuestProgress::JsonHistoryEvent> {
    using T = QuestProgress::JsonHistoryEvent;
    static constexpr auto value = object(
        "gameQuestId", &T::game_quest_id,
        "eventType", &T::event_type,
        "state", &T::state,
        "source", &T::source,
        "confidence", &T::confidence,
        "observedAt", &T::observed_at,
        "semanticEventKey", &T::semantic_event_key,
        "objectives", &T::objectives,
        "evidenceKind", &T::evidence_kind);
};

template <>
struct glz::meta<QuestProgress::JsonQuest> {
    using T = QuestProgress::JsonQuest;
    static constexpr auto value = object(
        "gameQuestId", &T::game_quest_id,
        "state", &T::state,
        "source", &T::source,
        "confidence", &T::confidence,
        "firstObservedAt", &T::first_observed_at,
        "lastObservedAt", &T::last_observed_at,
        "completedAt", &T::completed_at,
        "objectives", &T::objectives,
        "history", &T::history);
};

template <>
struct glz::meta<QuestProgress::JsonMission> {
    using T = QuestProgress::JsonMission;
    static constexpr auto value = object(
        "mapId", &T::map_id,
        "completedNormal", &T::completed_normal,
        "completedHard", &T::completed_hard,
        "bonusNormal", &T::bonus_normal,
        "bonusHard", &T::bonus_hard,
        "lastObservedAt", &T::last_observed_at);
};

template <>
struct glz::meta<QuestProgress::JsonCharacter> {
    using T = QuestProgress::JsonCharacter;
    static constexpr auto value = object(
        "characterKey", &T::character_key,
        "displayName", &T::display_name,
        "profession", &T::profession,
        "isPreSearing", &T::is_pre_searing,
        "firstObservedAt", &T::first_observed_at,
        "lastObservedAt", &T::last_observed_at,
        "quests", &T::quests,
        "missions", &T::missions);
};

template <>
struct glz::meta<QuestProgress::JsonAccountStore> {
    using T = QuestProgress::JsonAccountStore;
    static constexpr auto value = object(
        "storeFormat", &T::store_format,
        "storeVersion", &T::store_version,
        "accountKey", &T::account_key,
        "characters", &T::characters);
};

namespace QuestProgress {
namespace {

constexpr glz::opts kReadOpts{.error_on_unknown_keys = false};
constexpr glz::opts kWriteOpts{.prettify = true};

void AddDiag(CodecDiagnostics& d, std::string message)
{
    d.messages.push_back(std::move(message));
}

bool ParseState(std::string_view s, ProgressState& out)
{
    if (s == "unknown") { out = ProgressState::Unknown; return true; }
    if (s == "available") { out = ProgressState::Available; return true; }
    if (s == "active") { out = ProgressState::Active; return true; }
    if (s == "objective_progress") { out = ProgressState::ObjectiveProgress; return true; }
    if (s == "ready_for_reward") { out = ProgressState::ReadyForReward; return true; }
    if (s == "completed_observed") { out = ProgressState::CompletedObserved; return true; }
    if (s == "completed_manual") { out = ProgressState::CompletedManual; return true; }
    if (s == "abandoned_observed") { out = ProgressState::AbandonedObserved; return true; }
    return false;
}

bool ParseSource(std::string_view s, ProgressSource& out)
{
    if (s == "game_snapshot") { out = ProgressSource::GameSnapshot; return true; }
    if (s == "game_event") { out = ProgressSource::GameEvent; return true; }
    if (s == "mission_completion_data") { out = ProgressSource::MissionCompletionData; return true; }
    if (s == "toolbox_existing_data") { out = ProgressSource::ToolboxExistingData; return true; }
    if (s == "manual_user_input") { out = ProgressSource::ManualUserInput; return true; }
    if (s == "imported_history") { out = ProgressSource::ImportedHistory; return true; }
    if (s == "migration") { out = ProgressSource::Migration; return true; }
    return false;
}

bool ParseConfidence(std::string_view s, Confidence& out)
{
    if (s == "confirmed") { out = Confidence::Confirmed; return true; }
    if (s == "probable") { out = Confidence::Probable; return true; }
    if (s == "uncertain") { out = Confidence::Uncertain; return true; }
    if (s == "manual") { out = Confidence::Manual; return true; }
    return false;
}

bool ParseEventType(std::string_view s, HistoryEventType& out)
{
    if (s == "observation") { out = HistoryEventType::Observation; return true; }
    if (s == "presence_lost") { out = HistoryEventType::PresenceLost; return true; }
    if (s == "abandoned") { out = HistoryEventType::Abandoned; return true; }
    if (s == "completed") { out = HistoryEventType::Completed; return true; }
    return false;
}

bool ParseEvidenceKind(std::string_view s, EvidenceKind& out)
{
    if (s.empty() || s == "none") { out = EvidenceKind::None; return true; }
    if (s == "abandon") { out = EvidenceKind::Abandon; return true; }
    if (s == "reward") { out = EvidenceKind::Reward; return true; }
    if (s == "enquire_reward") { out = EvidenceKind::EnquireReward; return true; }
    return false;
}

bool RequireCanonicalTs(std::string_view ts, CodecDiagnostics& d, const char* field)
{
    if (!IsCanonicalUtcTimestamp(ts)) {
        AddDiag(d, std::string("non-canonical timestamp in ") + field + ": " + std::string(ts));
        return false;
    }
    return true;
}

bool ConvertObjective(const JsonObjective& in, ObjectiveObservation& out, CodecDiagnostics& d)
{
    out.index = in.index;
    out.completed = in.completed;
    if (!DecodeContentFromLeHex(in.encoded_content_hex, out.encoded_content)) {
        AddDiag(d, "invalid encoded_content_hex");
        return false;
    }
    out.content_fingerprint = FingerprintEncodedContent(out.encoded_content);
    if (!in.content_fingerprint.empty() && in.content_fingerprint != out.content_fingerprint) {
        // Prefer recomputed fingerprint; tolerate stale stored fingerprint with diagnostic.
        AddDiag(d, "content_fingerprint recomputed from encoded content");
    }
    return true;
}

JsonObjective ToJsonObjective(const ObjectiveObservation& in)
{
    JsonObjective out;
    out.index = in.index;
    out.completed = in.completed;
    out.encoded_content_hex = EncodeContentToLeHex(in.encoded_content);
    out.content_fingerprint = FingerprintEncodedContent(in.encoded_content);
    return out;
}

bool ConvertHistory(const JsonHistoryEvent& in, QuestHistoryEvent& out, CodecDiagnostics& d)
{
    out.game_quest_id = in.game_quest_id;
    if (!ParseEventType(in.event_type, out.event_type)
        || !ParseState(in.state, out.state)
        || !ParseSource(in.source, out.source)
        || !ParseConfidence(in.confidence, out.confidence)
        || !ParseEvidenceKind(in.evidence_kind, out.evidence_kind)) {
        AddDiag(d, "invalid history enum");
        return false;
    }
    if (!RequireCanonicalTs(in.observed_at, d, "history.observedAt")) {
        return false;
    }
    out.observed_at = in.observed_at;
    if (in.semantic_event_key.empty()) {
        AddDiag(d, "missing semanticEventKey");
        return false;
    }
    out.semantic_event_key = in.semantic_event_key;
    out.objectives.clear();
    for (const auto& jo : in.objectives) {
        ObjectiveObservation obj;
        if (!ConvertObjective(jo, obj, d)) {
            return false;
        }
        out.objectives.push_back(std::move(obj));
    }
    NormalizeObjectives(out.objectives);
    return true;
}

JsonHistoryEvent ToJsonHistory(const QuestHistoryEvent& in)
{
    JsonHistoryEvent out;
    out.game_quest_id = in.game_quest_id;
    out.event_type = ToString(in.event_type);
    out.state = ToString(in.state);
    out.source = ToString(in.source);
    out.confidence = ToString(in.confidence);
    out.observed_at = in.observed_at;
    out.semantic_event_key = in.semantic_event_key;
    out.evidence_kind = ToString(in.evidence_kind);
    auto objectives = in.objectives;
    NormalizeObjectives(objectives);
    for (const auto& obj : objectives) {
        out.objectives.push_back(ToJsonObjective(obj));
    }
    return out;
}

bool ConvertQuest(const JsonQuest& in, QuestProgress& out, CodecDiagnostics& d)
{
    out.game_quest_id = in.game_quest_id;
    if (!ParseState(in.state, out.state)
        || !ParseSource(in.source, out.source)
        || !ParseConfidence(in.confidence, out.confidence)) {
        AddDiag(d, "invalid quest enum");
        return false;
    }
    if (!RequireCanonicalTs(in.first_observed_at, d, "quest.firstObservedAt")
        || !RequireCanonicalTs(in.last_observed_at, d, "quest.lastObservedAt")) {
        return false;
    }
    out.first_observed_at = in.first_observed_at;
    out.last_observed_at = in.last_observed_at;
    if (in.completed_at) {
        if (!RequireCanonicalTs(*in.completed_at, d, "quest.completedAt")) {
            return false;
        }
        out.completed_at = *in.completed_at;
    }
    out.objectives.clear();
    for (const auto& jo : in.objectives) {
        ObjectiveObservation obj;
        if (!ConvertObjective(jo, obj, d)) {
            return false;
        }
        out.objectives.push_back(std::move(obj));
    }
    NormalizeObjectives(out.objectives);
    out.history.clear();
    for (const auto& jh : in.history) {
        QuestHistoryEvent ev;
        if (!ConvertHistory(jh, ev, d)) {
            return false;
        }
        out.history.push_back(std::move(ev));
    }
    return true;
}

JsonQuest ToJsonQuest(const QuestProgress& in)
{
    JsonQuest out;
    out.game_quest_id = in.game_quest_id;
    out.state = ToString(in.state);
    out.source = ToString(in.source);
    out.confidence = ToString(in.confidence);
    out.first_observed_at = in.first_observed_at;
    out.last_observed_at = in.last_observed_at;
    out.completed_at = in.completed_at;
    auto objectives = in.objectives;
    NormalizeObjectives(objectives);
    for (const auto& obj : objectives) {
        out.objectives.push_back(ToJsonObjective(obj));
    }
    auto history = in.history;
    std::sort(history.begin(), history.end(), [](const QuestHistoryEvent& a, const QuestHistoryEvent& b) {
        if (a.observed_at != b.observed_at) {
            return a.observed_at < b.observed_at;
        }
        return a.semantic_event_key < b.semantic_event_key;
    });
    for (const auto& ev : history) {
        out.history.push_back(ToJsonHistory(ev));
    }
    return out;
}

bool ConvertCharacter(const JsonCharacter& in, StoredCharacter& out, CodecDiagnostics& d)
{
    if (in.character_key.empty()) {
        AddDiag(d, "empty characterKey");
        return false;
    }
    out.character_key = in.character_key;
    out.display_name = in.display_name;
    out.profession = in.profession;
    out.is_pre_searing = in.is_pre_searing;
    if (!in.first_observed_at.empty() && !RequireCanonicalTs(in.first_observed_at, d, "character.firstObservedAt")) {
        return false;
    }
    if (!in.last_observed_at.empty() && !RequireCanonicalTs(in.last_observed_at, d, "character.lastObservedAt")) {
        return false;
    }
    out.first_observed_at = in.first_observed_at;
    out.last_observed_at = in.last_observed_at;
    out.quests.clear();
    for (const auto& jq : in.quests) {
        QuestProgress quest;
        if (!ConvertQuest(jq, quest, d)) {
            return false;
        }
        if (out.quests.contains(quest.game_quest_id)) {
            AddDiag(d, "duplicate gameQuestId in character");
            return false;
        }
        out.quests.emplace(quest.game_quest_id, std::move(quest));
    }
    out.missions.clear();
    for (const auto& jm : in.missions) {
        MissionRecord mission;
        mission.map_id = jm.map_id;
        mission.completed_normal = jm.completed_normal;
        mission.completed_hard = jm.completed_hard;
        mission.bonus_normal = jm.bonus_normal;
        mission.bonus_hard = jm.bonus_hard;
        if (!jm.last_observed_at.empty() && !RequireCanonicalTs(jm.last_observed_at, d, "mission.lastObservedAt")) {
            return false;
        }
        mission.last_observed_at = jm.last_observed_at;
        if (out.missions.contains(mission.map_id)) {
            AddDiag(d, "duplicate mapId in missions");
            return false;
        }
        out.missions.emplace(mission.map_id, std::move(mission));
    }
    return true;
}

JsonCharacter ToJsonCharacter(const StoredCharacter& in)
{
    JsonCharacter out;
    out.character_key = in.character_key;
    out.display_name = in.display_name;
    out.profession = in.profession;
    out.is_pre_searing = in.is_pre_searing;
    out.first_observed_at = in.first_observed_at;
    out.last_observed_at = in.last_observed_at;
    for (const auto& [id, quest] : in.quests) {
        (void)id;
        out.quests.push_back(ToJsonQuest(quest));
    }
    std::sort(out.quests.begin(), out.quests.end(), [](const JsonQuest& a, const JsonQuest& b) {
        return a.game_quest_id < b.game_quest_id;
    });
    for (const auto& [id, mission] : in.missions) {
        (void)id;
        JsonMission jm;
        jm.map_id = mission.map_id;
        jm.completed_normal = mission.completed_normal;
        jm.completed_hard = mission.completed_hard;
        jm.bonus_normal = mission.bonus_normal;
        jm.bonus_hard = mission.bonus_hard;
        jm.last_observed_at = mission.last_observed_at;
        out.missions.push_back(jm);
    }
    std::sort(out.missions.begin(), out.missions.end(), [](const JsonMission& a, const JsonMission& b) {
        return a.map_id < b.map_id;
    });
    return out;
}

bool Migrate_1_0_to_1_1(AccountProgressStore& store, CodecDiagnostics& d)
{
    CanonicalizeAccountStore(store);
    AddDiag(d, "migrated store minor 1.0 -> 1.1");
    store.store_version.minor = 1;
    return true;
}

} // namespace

std::string NormalizeAccountKey(std::string_view raw)
{
    std::string s;
    s.reserve(raw.size());
    for (char c : raw) {
        if (c == '{' || c == '}') {
            continue;
        }
        s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (!IsValidNormalizedAccountKey(s)) {
        return {};
    }
    return s;
}

bool IsValidNormalizedAccountKey(std::string_view key)
{
    // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    if (key.size() != 36) {
        return false;
    }
    for (size_t i = 0; i < key.size(); ++i) {
        const char c = key[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') {
                return false;
            }
            continue;
        }
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if (!hex) {
            return false;
        }
    }
    return true;
}

std::string EncodeContentToLeHex(std::u16string_view encoded)
{
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(encoded.size() * 4);
    size_t o = 0;
    for (char16_t ch : encoded) {
        const auto lo = static_cast<uint8_t>(ch & 0xff);
        const auto hi = static_cast<uint8_t>((ch >> 8) & 0xff);
        out[o++] = kHex[lo >> 4];
        out[o++] = kHex[lo & 0xf];
        out[o++] = kHex[hi >> 4];
        out[o++] = kHex[hi & 0xf];
    }
    return out;
}

bool DecodeContentFromLeHex(std::string_view hex, std::u16string& out)
{
    out.clear();
    if (hex.size() % 4 != 0) {
        return false;
    }
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    out.resize(hex.size() / 4);
    for (size_t i = 0, qi = 0; i < hex.size(); i += 4, ++qi) {
        const int a = nibble(hex[i]);
        const int b = nibble(hex[i + 1]);
        const int c = nibble(hex[i + 2]);
        const int d = nibble(hex[i + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0) {
            out.clear();
            return false;
        }
        const auto lo = static_cast<uint8_t>((a << 4) | b);
        const auto hi = static_cast<uint8_t>((c << 4) | d);
        out[qi] = static_cast<char16_t>(lo | (static_cast<uint16_t>(hi) << 8));
    }
    return true;
}

void CanonicalizeAccountStore(AccountProgressStore& store)
{
    for (auto& [ck, character] : store.characters) {
        (void)ck;
        for (auto& [qid, quest] : character.quests) {
            (void)qid;
            NormalizeObjectives(quest.objectives);
            for (auto& ev : quest.history) {
                NormalizeObjectives(ev.objectives);
            }
            std::sort(quest.history.begin(), quest.history.end(),
                [](const QuestHistoryEvent& a, const QuestHistoryEvent& b) {
                    if (a.observed_at != b.observed_at) {
                        return a.observed_at < b.observed_at;
                    }
                    return a.semantic_event_key < b.semantic_event_key;
                });
        }
    }
}

bool MigrateAccountStoreToCurrent(AccountProgressStore& store, CodecDiagnostics& diagnostics)
{
    if (store.store_format != kStoreFormatId) {
        AddDiag(diagnostics, "invalid storeFormat");
        return false;
    }
    if (store.store_version.major != kStoreFormatMajor) {
        AddDiag(diagnostics, "unsupported store major for migration");
        return false;
    }
    if (store.store_version.minor > kStoreFormatMinor) {
        AddDiag(diagnostics, "newer minor than supported");
        return false;
    }
    while (store.store_version.minor < kStoreFormatMinor) {
        if (store.store_version.minor == 0) {
            if (!Migrate_1_0_to_1_1(store, diagnostics)) {
                return false;
            }
            diagnostics.migrated = true;
            continue;
        }
        AddDiag(diagnostics, "missing migration step");
        return false;
    }
    CanonicalizeAccountStore(store);
    store.store_version.major = kStoreFormatMajor;
    store.store_version.minor = kStoreFormatMinor;
    return true;
}

CodecParseResult ParseAccountStoreJson(
    std::string_view utf8_json,
    std::string_view expected_account_key)
{
    CodecParseResult result;
    if (utf8_json.empty()) {
        result.status = CodecStatus::EmptyStore;
        AddDiag(result.diagnostics, "empty json");
        return result;
    }

    JsonAccountStore raw;
    if (auto ec = glz::read<kReadOpts>(raw, utf8_json); ec) {
        result.status = CodecStatus::ParseError;
        AddDiag(result.diagnostics, "glaze parse failed: " + glz::format_error(ec, utf8_json));
        return result;
    }

    if (raw.store_format != kStoreFormatId) {
        result.status = CodecStatus::ValidationError;
        AddDiag(result.diagnostics, "unexpected storeFormat");
        return result;
    }

    if (raw.store_version.major > kStoreFormatMajor) {
        result.status = CodecStatus::UnsupportedNewerMajor;
        AddDiag(result.diagnostics, "unsupported newer major");
        return result;
    }
    if (raw.store_version.major < kStoreFormatMajor) {
        result.status = CodecStatus::ValidationError;
        AddDiag(result.diagnostics, "unsupported older major");
        return result;
    }
    if (raw.store_version.minor > kStoreFormatMinor) {
        result.status = CodecStatus::UnsupportedNewerMajor;
        AddDiag(result.diagnostics, "unsupported newer minor treated as reject");
        return result;
    }

    const auto normalized_expected = NormalizeAccountKey(expected_account_key);
    const auto normalized_file = NormalizeAccountKey(raw.account_key);
    if (normalized_file.empty()) {
        result.status = CodecStatus::ValidationError;
        AddDiag(result.diagnostics, "invalid accountKey in file");
        return result;
    }
    if (!normalized_expected.empty() && normalized_expected != normalized_file) {
        result.status = CodecStatus::AccountKeyMismatch;
        AddDiag(result.diagnostics, "accountKey mismatch");
        return result;
    }

    AccountProgressStore store;
    store.store_format = kStoreFormatId;
    store.store_version.major = raw.store_version.major;
    store.store_version.minor = raw.store_version.minor;
    store.account_key = normalized_file;

    for (const auto& jc : raw.characters) {
        StoredCharacter character;
        if (!ConvertCharacter(jc, character, result.diagnostics)) {
            result.status = CodecStatus::ValidationError;
            return result;
        }
        if (store.characters.contains(character.character_key)) {
            result.status = CodecStatus::ValidationError;
            AddDiag(result.diagnostics, "duplicate characterKey");
            return result;
        }
        store.characters.emplace(character.character_key, std::move(character));
    }

    if (!MigrateAccountStoreToCurrent(store, result.diagnostics)) {
        result.status = CodecStatus::MigrationError;
        return result;
    }

    result.store = std::move(store);
    result.status = CodecStatus::Ok;
    return result;
}

CodecSerializeResult SerializeAccountStoreJson(const AccountProgressStore& store)
{
    CodecSerializeResult result;
    auto copy = store;
    CanonicalizeAccountStore(copy);
    copy.store_format = kStoreFormatId;
    copy.store_version.major = kStoreFormatMajor;
    copy.store_version.minor = kStoreFormatMinor;

    if (!IsValidNormalizedAccountKey(copy.account_key)) {
        result.status = CodecStatus::ValidationError;
        AddDiag(result.diagnostics, "invalid accountKey for serialize");
        return result;
    }

    JsonAccountStore raw;
    raw.store_format = copy.store_format;
    raw.store_version.major = copy.store_version.major;
    raw.store_version.minor = copy.store_version.minor;
    raw.account_key = copy.account_key;
    for (const auto& [ck, character] : copy.characters) {
        (void)ck;
        raw.characters.push_back(ToJsonCharacter(character));
    }
    std::sort(raw.characters.begin(), raw.characters.end(),
        [](const JsonCharacter& a, const JsonCharacter& b) {
            return a.character_key < b.character_key;
        });

    if (glz::write<kWriteOpts>(raw, result.utf8_json)) {
        result.status = CodecStatus::ParseError;
        AddDiag(result.diagnostics, "glaze write failed");
        return result;
    }
    result.status = CodecStatus::Ok;
    return result;
}

} // namespace QuestProgress
