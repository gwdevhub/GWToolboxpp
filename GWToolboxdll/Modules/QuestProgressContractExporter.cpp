#include <Modules/QuestProgressContractExporter.h>
#include <Modules/QuestSessionIdentity.h>

#include <glaze/glaze.hpp>

#include <algorithm>
#include <cstdint>
#include <map>

namespace QuestProgress {

struct ContractObjectiveJson {
    uint32_t objective_index = 0;
    bool is_completed = false;
    std::optional<std::string> text;
    std::optional<std::string> observed_at;
};

struct ContractHistoryJson {
    std::optional<std::string> event_id;
    std::string event_type;
    std::string observed_at;
    std::string state;
    std::string source;
    std::string confidence;
    std::vector<ContractObjectiveJson> objectives;
    std::optional<std::map<std::string, std::string>> payload;
};

struct ContractQuestJson {
    uint32_t game_quest_id = 0;
    std::optional<std::string> quest_name;
    std::string state;
    std::string source;
    std::string confidence;
    std::optional<std::string> first_observed_at;
    std::string observed_at;
    std::vector<ContractObjectiveJson> objectives;
    std::vector<ContractHistoryJson> history;
};

struct ContractCharacterJson {
    std::string character_key;
    std::string display_name;
    std::optional<bool> is_pre_searing;
    std::vector<ContractQuestJson> quests;
};

struct ContractProducerJson {
    std::string name;
    std::optional<std::string> version;
};

struct ContractEnvelopeJson {
    std::string contract;
    int contract_version = kContractVersion;
    ContractProducerJson producer;
    std::string exported_at;
    std::vector<ContractCharacterJson> characters;
};

namespace {

void AddDiag(ContractExportDiagnostics& d, std::string message)
{
    d.messages.push_back(std::move(message));
}

bool IsExportableSource(ProgressSource source)
{
    return source != ProgressSource::Migration;
}

std::vector<ContractObjectiveJson> MapObjectives(const std::vector<ObjectiveObservation>& in)
{
    std::vector<ContractObjectiveJson> out;
    out.reserve(in.size());
    for (const auto& obj : in) {
        ContractObjectiveJson row;
        row.objective_index = obj.index;
        row.is_completed = obj.completed;
        // encoded_content is EncString bytes — not decoded player text; omit optional text.
        out.push_back(std::move(row));
    }
    std::sort(out.begin(), out.end(), [](const ContractObjectiveJson& a, const ContractObjectiveJson& b) {
        return a.objective_index < b.objective_index;
    });
    return out;
}

std::optional<std::map<std::string, std::string>> MapEvidencePayload(EvidenceKind kind)
{
    if (kind == EvidenceKind::None) {
        return std::nullopt;
    }
    return std::map<std::string, std::string>{{"evidenceKind", ToString(kind)}};
}

std::optional<ContractHistoryJson> MapHistoryEvent(const QuestHistoryEvent& in, ContractExportDiagnostics& diag)
{
    if (!IsExportableSource(in.source)) {
        AddDiag(diag, "skipped history event with migration source");
        return std::nullopt;
    }
    ContractHistoryJson out;
    if (!in.semantic_event_key.empty()) {
        out.event_id = in.semantic_event_key;
    }
    out.event_type = ToString(in.event_type);
    out.observed_at = in.observed_at;
    out.state = ToString(in.state);
    out.source = ToString(in.source);
    out.confidence = ToString(in.confidence);
    out.objectives = MapObjectives(in.objectives);
    out.payload = MapEvidencePayload(in.evidence_kind);
    return out;
}

std::optional<ContractQuestJson> MapQuest(const QuestProgress& in, ContractExportDiagnostics& diag)
{
    if (in.game_quest_id == 0 || IsSyntheticQuestId(in.game_quest_id)) {
        ++diag.quests_skipped;
        return std::nullopt;
    }
    if (!IsExportableSource(in.source)) {
        ++diag.quests_skipped;
        AddDiag(diag, "skipped quest with migration source");
        return std::nullopt;
    }

    ContractQuestJson out;
    out.game_quest_id = in.game_quest_id;
    out.state = ToString(in.state);
    out.source = ToString(in.source);
    out.confidence = ToString(in.confidence);
    if (!in.first_observed_at.empty()) {
        out.first_observed_at = in.first_observed_at;
    }
    out.observed_at = in.last_observed_at;
    out.objectives = MapObjectives(in.objectives);

    for (const auto& hist : in.history) {
        if (auto mapped = MapHistoryEvent(hist, diag)) {
            out.history.push_back(std::move(*mapped));
            ++diag.history_events_exported;
        }
    }
    return out;
}

ContractCharacterJson MapCharacter(const StoredCharacter& in, ContractExportDiagnostics& diag)
{
    ContractCharacterJson out;
    out.character_key = in.character_key;
    // Contract requires non-empty displayName; identity remains characterKey only.
    out.display_name = in.display_name.empty() ? "Unknown" : in.display_name;
    if (in.display_name.empty()) {
        AddDiag(diag, "displayName empty; exported as Unknown (identity is characterKey)");
    }
    out.is_pre_searing = in.is_pre_searing;

    for (const auto& [id, quest] : in.quests) {
        (void)id;
        if (auto mapped = MapQuest(quest, diag)) {
            out.quests.push_back(std::move(*mapped));
            ++diag.quests_exported;
        }
    }
    return out;
}

bool ShouldExportCharacter(
    const std::string& map_key,
    const StoredCharacter& character,
    const ContractExportOptions& options,
    ContractExportDiagnostics& diag)
{
    const std::string effective_key =
        !character.character_key.empty() ? character.character_key : map_key;

    if (options.bound_character_key.empty()) {
        if (!IsValidPersistentCharacterKey(effective_key)) {
            ++diag.characters_skipped;
            AddDiag(diag, "skipped character with invalid persistent characterKey");
            return false;
        }
        return true;
    }

    if (effective_key != options.bound_character_key) {
        ++diag.characters_skipped;
        return false;
    }
    if (!IsValidPersistentCharacterKey(effective_key)) {
        ++diag.characters_skipped;
        AddDiag(diag, "bound characterKey is invalid; export refused");
        return false;
    }
    return true;
}

} // namespace

} // namespace QuestProgress

template <>
struct glz::meta<QuestProgress::ContractObjectiveJson> {
    using T = QuestProgress::ContractObjectiveJson;
    static constexpr auto value = object(
        "objectiveIndex", &T::objective_index,
        "isCompleted", &T::is_completed,
        "text", &T::text,
        "observedAt", &T::observed_at);
};

template <>
struct glz::meta<QuestProgress::ContractHistoryJson> {
    using T = QuestProgress::ContractHistoryJson;
    static constexpr auto value = object(
        "eventId", &T::event_id,
        "eventType", &T::event_type,
        "observedAt", &T::observed_at,
        "state", &T::state,
        "source", &T::source,
        "confidence", &T::confidence,
        "objectives", &T::objectives,
        "payload", &T::payload);
};

template <>
struct glz::meta<QuestProgress::ContractQuestJson> {
    using T = QuestProgress::ContractQuestJson;
    static constexpr auto value = object(
        "gameQuestId", &T::game_quest_id,
        "questName", &T::quest_name,
        "state", &T::state,
        "source", &T::source,
        "confidence", &T::confidence,
        "firstObservedAt", &T::first_observed_at,
        "observedAt", &T::observed_at,
        "objectives", &T::objectives,
        "history", &T::history);
};

template <>
struct glz::meta<QuestProgress::ContractCharacterJson> {
    using T = QuestProgress::ContractCharacterJson;
    static constexpr auto value = object(
        "characterKey", &T::character_key,
        "displayName", &T::display_name,
        "isPreSearing", &T::is_pre_searing,
        "quests", &T::quests);
};

template <>
struct glz::meta<QuestProgress::ContractProducerJson> {
    using T = QuestProgress::ContractProducerJson;
    static constexpr auto value = object("name", &T::name, "version", &T::version);
};

template <>
struct glz::meta<QuestProgress::ContractEnvelopeJson> {
    using T = QuestProgress::ContractEnvelopeJson;
    static constexpr auto value = object(
        "contract", &T::contract,
        "contractVersion", &T::contract_version,
        "producer", &T::producer,
        "exportedAt", &T::exported_at,
        "characters", &T::characters);
};

namespace QuestProgress {

ContractExportResult ExportAccountStoreToContractV1(
    const AccountProgressStore& store,
    const ContractExportOptions& options)
{
    ContractExportResult result;
    if (!IsCanonicalUtcTimestamp(options.exported_at_utc)) {
        result.status = ContractExportStatus::InvalidOptions;
        AddDiag(result.diagnostics, "exported_at_utc must be canonical UTC ISO-8601 with millis and Z");
        return result;
    }

    ContractEnvelopeJson envelope;
    envelope.contract = kContractId;
    envelope.contract_version = kContractVersion;
    envelope.producer.name = kContractProducerName;
    if (!options.producer_version.empty()) {
        envelope.producer.version = options.producer_version;
    }
    envelope.exported_at = options.exported_at_utc;

    for (const auto& [key, character] : store.characters) {
        if (character.character_key.empty() && key.empty()) {
            ++result.diagnostics.characters_skipped;
            AddDiag(result.diagnostics, "skipped character with empty characterKey");
            continue;
        }
        if (!ShouldExportCharacter(key, character, options, result.diagnostics)) {
            continue;
        }
        auto mapped = MapCharacter(character, result.diagnostics);
        if (mapped.character_key.empty()) {
            mapped.character_key = key;
        }
        envelope.characters.push_back(std::move(mapped));
        ++result.diagnostics.characters_exported;
    }

    if (!options.bound_character_key.empty() && result.diagnostics.characters_exported == 0) {
        result.status = ContractExportStatus::InvalidOptions;
        AddDiag(result.diagnostics, "bound characterKey not found or not exportable");
        result.utf8_json.clear();
        return result;
    }

    constexpr glz::opts kWriteOpts{.prettify = true};
    if (glz::write<kWriteOpts>(envelope, result.utf8_json)) {
        result.status = ContractExportStatus::SerializeError;
        AddDiag(result.diagnostics, "glaze serialize failed");
        result.utf8_json.clear();
        return result;
    }

    result.status = ContractExportStatus::Ok;
    return result;
}

} // namespace QuestProgress
