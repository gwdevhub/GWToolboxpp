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
    std::optional<std::string> accepted_at;
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

struct JsonTitle {
    uint32_t title_id = 0;
    uint32_t tier_index = 0;
    uint32_t current_points = 0;
    std::string last_observed_at;
};

struct JsonJourneyEvent {
    std::string kind;
    std::string subject_key;
    std::string observed_at;
    uint32_t title_id = 0;
    uint32_t tier_index = 0;
    uint32_t level = 0;
    uint32_t map_id = 0;
    uint32_t skill_id = 0;
    uint32_t hero_id = 0;
    uint32_t profession_id = 0;
    uint32_t percent = 0;
    uint32_t amount = 0;
};

struct JsonHomSnapshot {
    std::string hom_code;
    std::string observed_at;
    uint32_t resilience_points = 0;
    uint32_t fellowship_points = 0;
    uint32_t honor_points = 0;
    uint32_t valor_points = 0;
    uint32_t devotion_points = 0;
    std::vector<bool> resilience_dedicated;
    std::vector<bool> fellowship_dedicated;
    std::vector<bool> honor_dedicated;
    std::vector<bool> valor_dedicated;
    std::vector<uint32_t> devotion_counts;
};

struct JsonFactionTotals {
    uint32_t kurzick = 0;
    uint32_t luxon = 0;
    uint32_t balthazar = 0;
    uint32_t imperial = 0;
};

struct JsonIdSetBaseline {
    std::string state;
    std::optional<std::vector<uint32_t>> ids;
};

struct JsonFlagBaseline {
    std::string state;
    std::optional<bool> unlocked;
};

struct JsonStateOnlyBaseline {
    std::string state;
};

struct JsonPercentBaseline {
    std::string state;
    std::optional<uint32_t> percent;
};

struct JsonCharacterJourneyBaselines {
    std::optional<JsonIdSetBaseline> maps;
    std::optional<JsonIdSetBaseline> character_skills;
    std::optional<JsonIdSetBaseline> heroes;
    std::optional<JsonIdSetBaseline> professions;
    std::optional<JsonIdSetBaseline> vanquish_areas;
    std::optional<JsonFlagBaseline> hard_mode;
    std::optional<JsonStateOnlyBaseline> skill_points;
    std::optional<JsonStateOnlyBaseline> factions;
    std::optional<JsonStateOnlyBaseline> hall_of_monuments;
    std::optional<JsonPercentBaseline> cartography;
};

struct JsonCharacter {
    std::string character_key;
    std::string display_name;
    std::string profession;
    std::string secondary_profession;
    std::optional<bool> is_pre_searing;
    std::optional<bool> is_pvp;
    std::optional<uint32_t> experience_total;
    std::optional<uint32_t> skill_points_earned;
    std::optional<JsonFactionTotals> faction_totals;
    std::optional<JsonHomSnapshot> hall_of_monuments;
    std::string first_observed_at;
    std::string last_observed_at;
    std::vector<JsonQuest> quests;
    std::vector<JsonMission> missions;
    std::vector<JsonTitle> titles;
    std::optional<uint32_t> last_known_level;
    std::optional<uint32_t> last_map_id;
    std::vector<JsonJourneyEvent> journey_events;
    std::optional<JsonCharacterJourneyBaselines> journey_baselines;
};

struct JsonAccountStore {
    std::string store_format;
    JsonStoreVersion store_version{};
    std::string account_key;
    std::optional<JsonIdSetBaseline> account_skill_baseline;
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
        "acceptedAt", &T::accepted_at,
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
struct glz::meta<QuestProgress::JsonTitle> {
    using T = QuestProgress::JsonTitle;
    static constexpr auto value = object(
        "titleId", &T::title_id,
        "tierIndex", &T::tier_index,
        "currentPoints", &T::current_points,
        "lastObservedAt", &T::last_observed_at);
};

template <>
struct glz::meta<QuestProgress::JsonJourneyEvent> {
    using T = QuestProgress::JsonJourneyEvent;
    static constexpr auto value = object(
        "kind", &T::kind,
        "subjectKey", &T::subject_key,
        "observedAt", &T::observed_at,
        "titleId", &T::title_id,
        "tierIndex", &T::tier_index,
        "level", &T::level,
        "mapId", &T::map_id,
        "skillId", &T::skill_id,
        "heroId", &T::hero_id,
        "professionId", &T::profession_id,
        "percent", &T::percent,
        "amount", &T::amount);
};

template <>
struct glz::meta<QuestProgress::JsonHomSnapshot> {
    using T = QuestProgress::JsonHomSnapshot;
    static constexpr auto value = object(
        "homCode", &T::hom_code,
        "observedAt", &T::observed_at,
        "resiliencePoints", &T::resilience_points,
        "fellowshipPoints", &T::fellowship_points,
        "honorPoints", &T::honor_points,
        "valorPoints", &T::valor_points,
        "devotionPoints", &T::devotion_points,
        "resilienceDedicated", &T::resilience_dedicated,
        "fellowshipDedicated", &T::fellowship_dedicated,
        "honorDedicated", &T::honor_dedicated,
        "valorDedicated", &T::valor_dedicated,
        "devotionCounts", &T::devotion_counts);
};

template <>
struct glz::meta<QuestProgress::JsonFactionTotals> {
    using T = QuestProgress::JsonFactionTotals;
    static constexpr auto value = object(
        "kurzick", &T::kurzick,
        "luxon", &T::luxon,
        "balthazar", &T::balthazar,
        "imperial", &T::imperial);
};

template <>
struct glz::meta<QuestProgress::JsonIdSetBaseline> {
    using T = QuestProgress::JsonIdSetBaseline;
    static constexpr auto value = object(
        "state", &T::state,
        "ids", &T::ids);
};

template <>
struct glz::meta<QuestProgress::JsonFlagBaseline> {
    using T = QuestProgress::JsonFlagBaseline;
    static constexpr auto value = object(
        "state", &T::state,
        "unlocked", &T::unlocked);
};

template <>
struct glz::meta<QuestProgress::JsonStateOnlyBaseline> {
    using T = QuestProgress::JsonStateOnlyBaseline;
    static constexpr auto value = object("state", &T::state);
};

template <>
struct glz::meta<QuestProgress::JsonPercentBaseline> {
    using T = QuestProgress::JsonPercentBaseline;
    static constexpr auto value = object(
        "state", &T::state,
        "percent", &T::percent);
};

template <>
struct glz::meta<QuestProgress::JsonCharacterJourneyBaselines> {
    using T = QuestProgress::JsonCharacterJourneyBaselines;
    static constexpr auto value = object(
        "maps", &T::maps,
        "characterSkills", &T::character_skills,
        "heroes", &T::heroes,
        "professions", &T::professions,
        "vanquishAreas", &T::vanquish_areas,
        "hardMode", &T::hard_mode,
        "skillPoints", &T::skill_points,
        "factions", &T::factions,
        "hallOfMonuments", &T::hall_of_monuments,
        "cartography", &T::cartography);
};

template <>
struct glz::meta<QuestProgress::JsonCharacter> {
    using T = QuestProgress::JsonCharacter;
    static constexpr auto value = object(
        "characterKey", &T::character_key,
        "displayName", &T::display_name,
        "profession", &T::profession,
        "secondaryProfession", &T::secondary_profession,
        "isPreSearing", &T::is_pre_searing,
        "isPvp", &T::is_pvp,
        "experienceTotal", &T::experience_total,
        "skillPointsEarned", &T::skill_points_earned,
        "factionTotals", &T::faction_totals,
        "hallOfMonuments", &T::hall_of_monuments,
        "firstObservedAt", &T::first_observed_at,
        "lastObservedAt", &T::last_observed_at,
        "quests", &T::quests,
        "missions", &T::missions,
        "titles", &T::titles,
        "lastKnownLevel", &T::last_known_level,
        "lastMapId", &T::last_map_id,
        "journeyEvents", &T::journey_events,
        "journeyBaselines", &T::journey_baselines);
};

template <>
struct glz::meta<QuestProgress::JsonAccountStore> {
    using T = QuestProgress::JsonAccountStore;
    static constexpr auto value = object(
        "storeFormat", &T::store_format,
        "storeVersion", &T::store_version,
        "accountKey", &T::account_key,
        "accountSkillBaseline", &T::account_skill_baseline,
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

bool ParseBaselineSealState(std::string_view s, JourneyBaselineSealState& out)
{
    if (s == "unset") {
        out = JourneyBaselineSealState::Unset;
        return true;
    }
    if (s == "sealed") {
        out = JourneyBaselineSealState::Sealed;
        return true;
    }
    return false;
}

bool IsKnownBaselineSealState(JourneyBaselineSealState state)
{
    return state == JourneyBaselineSealState::Unset || state == JourneyBaselineSealState::Sealed;
}

bool BaselineSealStateToString(JourneyBaselineSealState state, const char*& out)
{
    switch (state) {
        case JourneyBaselineSealState::Unset:
            out = "unset";
            return true;
        case JourneyBaselineSealState::Sealed:
            out = "sealed";
            return true;
    }
    return false;
}

void SortUniqueIds(std::vector<uint32_t>& ids)
{
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
}

void CanonicalizeIdSetBaseline(IdSetJourneyBaseline& baseline)
{
    if (baseline.state == JourneyBaselineSealState::Unset) {
        baseline.ids.clear();
        return;
    }
    SortUniqueIds(baseline.ids);
}

void CanonicalizeFlagBaseline(FlagJourneyBaseline& baseline)
{
    if (baseline.state == JourneyBaselineSealState::Unset) {
        baseline.unlocked = false;
    }
}

void CanonicalizePercentBaseline(PercentJourneyBaseline& baseline)
{
    if (baseline.state == JourneyBaselineSealState::Unset) {
        baseline.percent = 0;
    }
}

void CanonicalizeCharacterBaselines(CharacterJourneyBaselines& baselines)
{
    CanonicalizeIdSetBaseline(baselines.maps);
    CanonicalizeIdSetBaseline(baselines.character_skills);
    CanonicalizeIdSetBaseline(baselines.heroes);
    CanonicalizeIdSetBaseline(baselines.professions);
    CanonicalizeIdSetBaseline(baselines.vanquish_areas);
    CanonicalizeFlagBaseline(baselines.hard_mode);
    CanonicalizePercentBaseline(baselines.cartography);
}

bool ValidateIdSetBaseline(
    const IdSetJourneyBaseline& in,
    CodecDiagnostics& d,
    std::string_view path)
{
    if (!IsKnownBaselineSealState(in.state)) {
        AddDiag(d, std::string(path) + " unknown seal state");
        return false;
    }
    if (in.state == JourneyBaselineSealState::Unset && !in.ids.empty()) {
        AddDiag(d, std::string(path) + " unset baseline must not carry ids");
        return false;
    }
    return true;
}

bool ValidateFlagBaseline(
    const FlagJourneyBaseline& in,
    CodecDiagnostics& d,
    std::string_view path)
{
    if (!IsKnownBaselineSealState(in.state)) {
        AddDiag(d, std::string(path) + " unknown seal state");
        return false;
    }
    if (in.state == JourneyBaselineSealState::Unset && in.unlocked) {
        AddDiag(d, std::string(path) + " unset baseline must not carry unlocked");
        return false;
    }
    return true;
}

bool ValidateStateOnlyBaseline(
    const StateOnlyJourneyBaseline& in,
    CodecDiagnostics& d,
    std::string_view path)
{
    if (!IsKnownBaselineSealState(in.state)) {
        AddDiag(d, std::string(path) + " unknown seal state");
        return false;
    }
    return true;
}

bool ValidatePercentBaseline(
    const PercentJourneyBaseline& in,
    CodecDiagnostics& d,
    std::string_view path)
{
    if (!IsKnownBaselineSealState(in.state)) {
        AddDiag(d, std::string(path) + " unknown seal state");
        return false;
    }
    if (in.state == JourneyBaselineSealState::Unset && in.percent != 0) {
        AddDiag(d, std::string(path) + " unset baseline must not carry percent");
        return false;
    }
    if (in.state == JourneyBaselineSealState::Sealed && in.percent > 100) {
        AddDiag(d, std::string(path) + " cartography percent must be 0..100");
        return false;
    }
    return true;
}

bool ValidateCharacterBaselines(
    const CharacterJourneyBaselines& in,
    CodecDiagnostics& d,
    std::string_view character_path)
{
    const auto path = [&](std::string_view leaf) {
        return std::string(character_path) + ".journeyBaselines." + std::string(leaf);
    };
    return ValidateIdSetBaseline(in.maps, d, path("maps"))
        && ValidateIdSetBaseline(in.character_skills, d, path("characterSkills"))
        && ValidateIdSetBaseline(in.heroes, d, path("heroes"))
        && ValidateIdSetBaseline(in.professions, d, path("professions"))
        && ValidateIdSetBaseline(in.vanquish_areas, d, path("vanquishAreas"))
        && ValidateFlagBaseline(in.hard_mode, d, path("hardMode"))
        && ValidateStateOnlyBaseline(in.skill_points, d, path("skillPoints"))
        && ValidateStateOnlyBaseline(in.factions, d, path("factions"))
        && ValidateStateOnlyBaseline(in.hall_of_monuments, d, path("hallOfMonuments"))
        && ValidatePercentBaseline(in.cartography, d, path("cartography"));
}

bool ValidateAccountStoreBaselines(const AccountProgressStore& store, CodecDiagnostics& d)
{
    if (!ValidateIdSetBaseline(store.account_skill_baseline, d, "accountSkillBaseline")) {
        return false;
    }
    for (const auto& [ck, character] : store.characters) {
        if (!ValidateCharacterBaselines(character.journey_baselines, d, "characters." + ck)) {
            return false;
        }
    }
    return true;
}

bool ConvertIdSetBaseline(
    const JsonIdSetBaseline& in,
    IdSetJourneyBaseline& out,
    CodecDiagnostics& d,
    std::string_view path)
{
    if (!ParseBaselineSealState(in.state, out.state)) {
        AddDiag(d, std::string(path) + " unknown seal state");
        return false;
    }
    if (out.state == JourneyBaselineSealState::Unset) {
        if (in.ids.has_value() && !in.ids->empty()) {
            AddDiag(d, std::string(path) + " unset baseline must not carry ids");
            return false;
        }
        out.ids.clear();
        return true;
    }
    if (!in.ids.has_value()) {
        AddDiag(d, std::string(path) + " sealed baseline requires ids");
        return false;
    }
    out.ids = *in.ids;
    SortUniqueIds(out.ids);
    return true;
}

bool ConvertFlagBaseline(
    const JsonFlagBaseline& in,
    FlagJourneyBaseline& out,
    CodecDiagnostics& d,
    std::string_view path)
{
    if (!ParseBaselineSealState(in.state, out.state)) {
        AddDiag(d, std::string(path) + " unknown seal state");
        return false;
    }
    if (out.state == JourneyBaselineSealState::Unset) {
        if (in.unlocked.has_value()) {
            AddDiag(d, std::string(path) + " unset baseline must not carry unlocked");
            return false;
        }
        out.unlocked = false;
        return true;
    }
    if (!in.unlocked.has_value()) {
        AddDiag(d, std::string(path) + " sealed hardMode requires unlocked");
        return false;
    }
    out.unlocked = *in.unlocked;
    return true;
}

bool ConvertStateOnlyBaseline(
    const JsonStateOnlyBaseline& in,
    StateOnlyJourneyBaseline& out,
    CodecDiagnostics& d,
    std::string_view path)
{
    if (!ParseBaselineSealState(in.state, out.state)) {
        AddDiag(d, std::string(path) + " unknown seal state");
        return false;
    }
    return true;
}

bool ConvertPercentBaseline(
    const JsonPercentBaseline& in,
    PercentJourneyBaseline& out,
    CodecDiagnostics& d,
    std::string_view path)
{
    if (!ParseBaselineSealState(in.state, out.state)) {
        AddDiag(d, std::string(path) + " unknown seal state");
        return false;
    }
    if (out.state == JourneyBaselineSealState::Unset) {
        if (in.percent.has_value()) {
            AddDiag(d, std::string(path) + " unset baseline must not carry percent");
            return false;
        }
        out.percent = 0;
        return true;
    }
    if (!in.percent.has_value()) {
        AddDiag(d, std::string(path) + " sealed cartography requires percent");
        return false;
    }
    if (*in.percent > 100) {
        AddDiag(d, std::string(path) + " cartography percent must be 0..100");
        return false;
    }
    out.percent = *in.percent;
    return true;
}

bool ConvertCharacterBaselines(
    const JsonCharacterJourneyBaselines& in,
    CharacterJourneyBaselines& out,
    CodecDiagnostics& d)
{
    if (in.maps.has_value()
        && !ConvertIdSetBaseline(*in.maps, out.maps, d, "journeyBaselines.maps")) {
        return false;
    }
    if (in.character_skills.has_value()
        && !ConvertIdSetBaseline(
               *in.character_skills, out.character_skills, d, "journeyBaselines.characterSkills")) {
        return false;
    }
    if (in.heroes.has_value()
        && !ConvertIdSetBaseline(*in.heroes, out.heroes, d, "journeyBaselines.heroes")) {
        return false;
    }
    if (in.professions.has_value()
        && !ConvertIdSetBaseline(
               *in.professions, out.professions, d, "journeyBaselines.professions")) {
        return false;
    }
    if (in.vanquish_areas.has_value()
        && !ConvertIdSetBaseline(
               *in.vanquish_areas, out.vanquish_areas, d, "journeyBaselines.vanquishAreas")) {
        return false;
    }
    if (in.hard_mode.has_value()
        && !ConvertFlagBaseline(*in.hard_mode, out.hard_mode, d, "journeyBaselines.hardMode")) {
        return false;
    }
    if (in.skill_points.has_value()
        && !ConvertStateOnlyBaseline(
               *in.skill_points, out.skill_points, d, "journeyBaselines.skillPoints")) {
        return false;
    }
    if (in.factions.has_value()
        && !ConvertStateOnlyBaseline(*in.factions, out.factions, d, "journeyBaselines.factions")) {
        return false;
    }
    if (in.hall_of_monuments.has_value()
        && !ConvertStateOnlyBaseline(
               *in.hall_of_monuments,
               out.hall_of_monuments,
               d,
               "journeyBaselines.hallOfMonuments")) {
        return false;
    }
    if (in.cartography.has_value()
        && !ConvertPercentBaseline(
               *in.cartography, out.cartography, d, "journeyBaselines.cartography")) {
        return false;
    }
    return true;
}

JsonIdSetBaseline ToJsonIdSetBaseline(const IdSetJourneyBaseline& in)
{
    JsonIdSetBaseline out;
    const char* state = nullptr;
    BaselineSealStateToString(in.state, state);
    out.state = state;
    if (in.state == JourneyBaselineSealState::Sealed) {
        out.ids = in.ids;
    }
    return out;
}

JsonFlagBaseline ToJsonFlagBaseline(const FlagJourneyBaseline& in)
{
    JsonFlagBaseline out;
    const char* state = nullptr;
    BaselineSealStateToString(in.state, state);
    out.state = state;
    if (in.state == JourneyBaselineSealState::Sealed) {
        out.unlocked = in.unlocked;
    }
    return out;
}

JsonStateOnlyBaseline ToJsonStateOnlyBaseline(const StateOnlyJourneyBaseline& in)
{
    JsonStateOnlyBaseline out;
    const char* state = nullptr;
    BaselineSealStateToString(in.state, state);
    out.state = state;
    return out;
}

JsonPercentBaseline ToJsonPercentBaseline(const PercentJourneyBaseline& in)
{
    JsonPercentBaseline out;
    const char* state = nullptr;
    BaselineSealStateToString(in.state, state);
    out.state = state;
    if (in.state == JourneyBaselineSealState::Sealed) {
        out.percent = in.percent;
    }
    return out;
}

JsonCharacterJourneyBaselines ToJsonCharacterBaselines(const CharacterJourneyBaselines& in)
{
    JsonCharacterJourneyBaselines out;
    out.maps = ToJsonIdSetBaseline(in.maps);
    out.character_skills = ToJsonIdSetBaseline(in.character_skills);
    out.heroes = ToJsonIdSetBaseline(in.heroes);
    out.professions = ToJsonIdSetBaseline(in.professions);
    out.vanquish_areas = ToJsonIdSetBaseline(in.vanquish_areas);
    out.hard_mode = ToJsonFlagBaseline(in.hard_mode);
    out.skill_points = ToJsonStateOnlyBaseline(in.skill_points);
    out.factions = ToJsonStateOnlyBaseline(in.factions);
    out.hall_of_monuments = ToJsonStateOnlyBaseline(in.hall_of_monuments);
    out.cartography = ToJsonPercentBaseline(in.cartography);
    return out;
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
    if (s == "accepted") { out = EvidenceKind::Accepted; return true; }
    if (s == "chat_reward") { out = EvidenceKind::ChatReward; return true; }
    if (s == "chat_updated") { out = EvidenceKind::ChatUpdated; return true; }
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
    if (in.accepted_at) {
        if (!RequireCanonicalTs(*in.accepted_at, d, "quest.acceptedAt")) {
            return false;
        }
        out.accepted_at = *in.accepted_at;
    }
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
    out.accepted_at = in.accepted_at;
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
    out.secondary_profession = in.secondary_profession;
    out.is_pre_searing = in.is_pre_searing;
    out.is_pvp = in.is_pvp;
    out.experience_total = in.experience_total;
    out.skill_points_earned = in.skill_points_earned;
    if (in.faction_totals.has_value()) {
        FactionTotalsRecord totals;
        totals.kurzick = in.faction_totals->kurzick;
        totals.luxon = in.faction_totals->luxon;
        totals.balthazar = in.faction_totals->balthazar;
        totals.imperial = in.faction_totals->imperial;
        out.faction_totals = totals;
    }
    if (in.hall_of_monuments.has_value()) {
        HomSnapshotRecord hom;
        hom.hom_code = in.hall_of_monuments->hom_code;
        hom.observed_at = in.hall_of_monuments->observed_at;
        if (!hom.observed_at.empty()
            && !RequireCanonicalTs(hom.observed_at, d, "hallOfMonuments.observedAt")) {
            return false;
        }
        hom.resilience_points = in.hall_of_monuments->resilience_points;
        hom.fellowship_points = in.hall_of_monuments->fellowship_points;
        hom.honor_points = in.hall_of_monuments->honor_points;
        hom.valor_points = in.hall_of_monuments->valor_points;
        hom.devotion_points = in.hall_of_monuments->devotion_points;
        for (const bool dedicated : in.hall_of_monuments->resilience_dedicated) {
            hom.resilience_dedicated.push_back(dedicated ? 1u : 0u);
        }
        for (const bool dedicated : in.hall_of_monuments->fellowship_dedicated) {
            hom.fellowship_dedicated.push_back(dedicated ? 1u : 0u);
        }
        for (const bool dedicated : in.hall_of_monuments->honor_dedicated) {
            hom.honor_dedicated.push_back(dedicated ? 1u : 0u);
        }
        for (const bool dedicated : in.hall_of_monuments->valor_dedicated) {
            hom.valor_dedicated.push_back(dedicated ? 1u : 0u);
        }
        hom.devotion_counts = in.hall_of_monuments->devotion_counts;
        out.hall_of_monuments = std::move(hom);
    }
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
    out.titles.clear();
    for (const auto& jt : in.titles) {
        TitleStateRecord title;
        title.title_id = jt.title_id;
        title.tier_index = jt.tier_index;
        title.current_points = jt.current_points;
        if (!jt.last_observed_at.empty()
            && !RequireCanonicalTs(jt.last_observed_at, d, "title.lastObservedAt")) {
            return false;
        }
        title.last_observed_at = jt.last_observed_at;
        if (title.title_id == 0) {
            AddDiag(d, "titleId must be non-zero");
            return false;
        }
        if (out.titles.contains(title.title_id)) {
            AddDiag(d, "duplicate titleId in character");
            return false;
        }
        out.titles.emplace(title.title_id, std::move(title));
    }
    out.last_known_level = in.last_known_level;
    out.last_map_id = in.last_map_id;
    out.journey_events.clear();
    for (const auto& je : in.journey_events) {
        JourneyEventRecord event;
        event.kind = je.kind;
        event.subject_key = je.subject_key;
        event.title_id = je.title_id;
        event.tier_index = je.tier_index;
        event.level = je.level;
        event.map_id = je.map_id;
        event.skill_id = je.skill_id;
        event.hero_id = je.hero_id;
        event.profession_id = je.profession_id;
        event.percent = je.percent;
        event.amount = je.amount;
        if (event.kind.empty() || event.subject_key.empty()) {
            AddDiag(d, "journey event missing kind or subjectKey");
            return false;
        }
        if (je.observed_at.empty() || !RequireCanonicalTs(je.observed_at, d, "journeyEvent.observedAt")) {
            return false;
        }
        event.observed_at = je.observed_at;
        out.journey_events.push_back(std::move(event));
    }
    if (in.journey_baselines.has_value()) {
        if (!ConvertCharacterBaselines(*in.journey_baselines, out.journey_baselines, d)) {
            return false;
        }
    }
    return true;
}

JsonCharacter ToJsonCharacter(const StoredCharacter& in)
{
    JsonCharacter out;
    out.character_key = in.character_key;
    out.display_name = in.display_name;
    out.profession = in.profession;
    out.secondary_profession = in.secondary_profession;
    out.is_pre_searing = in.is_pre_searing;
    out.is_pvp = in.is_pvp;
    out.experience_total = in.experience_total;
    out.skill_points_earned = in.skill_points_earned;
    if (in.faction_totals.has_value()) {
        JsonFactionTotals totals;
        totals.kurzick = in.faction_totals->kurzick;
        totals.luxon = in.faction_totals->luxon;
        totals.balthazar = in.faction_totals->balthazar;
        totals.imperial = in.faction_totals->imperial;
        out.faction_totals = totals;
    }
    if (in.hall_of_monuments.has_value()) {
        JsonHomSnapshot hom;
        hom.hom_code = in.hall_of_monuments->hom_code;
        hom.observed_at = in.hall_of_monuments->observed_at;
        hom.resilience_points = in.hall_of_monuments->resilience_points;
        hom.fellowship_points = in.hall_of_monuments->fellowship_points;
        hom.honor_points = in.hall_of_monuments->honor_points;
        hom.valor_points = in.hall_of_monuments->valor_points;
        hom.devotion_points = in.hall_of_monuments->devotion_points;
        for (const auto dedicated : in.hall_of_monuments->resilience_dedicated) {
            hom.resilience_dedicated.push_back(dedicated != 0);
        }
        for (const auto dedicated : in.hall_of_monuments->fellowship_dedicated) {
            hom.fellowship_dedicated.push_back(dedicated != 0);
        }
        for (const auto dedicated : in.hall_of_monuments->honor_dedicated) {
            hom.honor_dedicated.push_back(dedicated != 0);
        }
        for (const auto dedicated : in.hall_of_monuments->valor_dedicated) {
            hom.valor_dedicated.push_back(dedicated != 0);
        }
        hom.devotion_counts = in.hall_of_monuments->devotion_counts;
        out.hall_of_monuments = std::move(hom);
    }
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
    for (const auto& [id, title] : in.titles) {
        (void)id;
        JsonTitle jt;
        jt.title_id = title.title_id;
        jt.tier_index = title.tier_index;
        jt.current_points = title.current_points;
        jt.last_observed_at = title.last_observed_at;
        out.titles.push_back(jt);
    }
    std::sort(out.titles.begin(), out.titles.end(), [](const JsonTitle& a, const JsonTitle& b) {
        return a.title_id < b.title_id;
    });
    out.last_known_level = in.last_known_level;
    out.last_map_id = in.last_map_id;
    for (const auto& ev : in.journey_events) {
        JsonJourneyEvent je;
        je.kind = ev.kind;
        je.subject_key = ev.subject_key;
        je.observed_at = ev.observed_at;
        je.title_id = ev.title_id;
        je.tier_index = ev.tier_index;
        je.level = ev.level;
        je.map_id = ev.map_id;
        je.skill_id = ev.skill_id;
        je.hero_id = ev.hero_id;
        je.profession_id = ev.profession_id;
        je.percent = ev.percent;
        je.amount = ev.amount;
        out.journey_events.push_back(je);
    }
    std::sort(out.journey_events.begin(), out.journey_events.end(),
        [](const JsonJourneyEvent& a, const JsonJourneyEvent& b) {
            if (a.observed_at != b.observed_at) {
                return a.observed_at < b.observed_at;
            }
            if (a.kind != b.kind) {
                return a.kind < b.kind;
            }
            return a.subject_key < b.subject_key;
        });
    out.journey_baselines = ToJsonCharacterBaselines(in.journey_baselines);
    return out;
}

bool Migrate_1_0_to_1_1(AccountProgressStore& store, CodecDiagnostics& d)
{
    CanonicalizeAccountStore(store);
    AddDiag(d, "migrated store minor 1.0 -> 1.1");
    store.store_version.minor = 1;
    return true;
}

bool Migrate_1_1_to_1_2(AccountProgressStore& store, CodecDiagnostics& d)
{
    for (auto& [ck, character] : store.characters) {
        (void)ck;
        character.journey_baselines = CharacterJourneyBaselines{};
    }
    store.account_skill_baseline = IdSetJourneyBaseline{};
    CanonicalizeAccountStore(store);
    AddDiag(d, "migrated store minor 1.1 -> 1.2");
    store.store_version.minor = 2;
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
        std::sort(character.journey_events.begin(), character.journey_events.end(),
            [](const JourneyEventRecord& a, const JourneyEventRecord& b) {
                if (a.observed_at != b.observed_at) {
                    return a.observed_at < b.observed_at;
                }
                if (a.kind != b.kind) {
                    return a.kind < b.kind;
                }
                return a.subject_key < b.subject_key;
            });
        CanonicalizeCharacterBaselines(character.journey_baselines);
    }
    CanonicalizeIdSetBaseline(store.account_skill_baseline);
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
        if (store.store_version.minor == 1) {
            if (!Migrate_1_1_to_1_2(store, diagnostics)) {
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
    if (raw.account_skill_baseline.has_value()) {
        if (!ConvertIdSetBaseline(
                *raw.account_skill_baseline,
                store.account_skill_baseline,
                result.diagnostics,
                "accountSkillBaseline")) {
            result.status = CodecStatus::ValidationError;
            return result;
        }
    }

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
    if (!ValidateAccountStoreBaselines(copy, result.diagnostics)) {
        result.status = CodecStatus::ValidationError;
        result.utf8_json.clear();
        return result;
    }
    CanonicalizeAccountStore(copy);
    copy.store_format = kStoreFormatId;
    copy.store_version.major = kStoreFormatMajor;
    copy.store_version.minor = kStoreFormatMinor;

    if (!IsValidNormalizedAccountKey(copy.account_key)) {
        result.status = CodecStatus::ValidationError;
        result.utf8_json.clear();
        AddDiag(result.diagnostics, "invalid accountKey for serialize");
        return result;
    }

    JsonAccountStore raw;
    raw.store_format = copy.store_format;
    raw.store_version.major = copy.store_version.major;
    raw.store_version.minor = copy.store_version.minor;
    raw.account_key = copy.account_key;
    raw.account_skill_baseline = ToJsonIdSetBaseline(copy.account_skill_baseline);
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
        result.utf8_json.clear();
        AddDiag(result.diagnostics, "glaze write failed");
        return result;
    }
    result.status = CodecStatus::Ok;
    return result;
}

} // namespace QuestProgress
