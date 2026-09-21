#pragma once

// Quest Progress Contract v1 exporter (observational interchange).
// Converts AccountProgressStore → Contract JSON. Not the internal store format.

#include <Modules/QuestProgressJsonCodec.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace QuestProgress {

inline constexpr char kContractId[] = "guild-wars-quest-progress";
inline constexpr int kContractVersion = 1;
inline constexpr char kContractProducerName[] = "GWToolboxpp";

struct ContractExportOptions {
    // Required: canonical UTC ISO-8601 with trailing Z.
    std::string exported_at_utc;
    // Metadata only — never part of Contract event identity.
    std::string producer_version;
    // When non-empty, export only this persistent characterKey (never displayName).
    std::string bound_character_key;
};

struct ContractExportDiagnostics {
    std::vector<std::string> messages;
    size_t characters_exported = 0;
    size_t characters_skipped = 0;
    size_t quests_exported = 0;
    size_t history_events_exported = 0;
    size_t quests_skipped = 0;
};

enum class ContractExportStatus : uint8_t {
    Ok = 0,
    InvalidOptions,
    SerializeError,
};

struct ContractExportResult {
    ContractExportStatus status = ContractExportStatus::InvalidOptions;
    std::string utf8_json;
    ContractExportDiagnostics diagnostics;
};

// Pure mapping: AccountProgressStore → Contract v1 UTF-8 JSON.
// Skips synthetic quest ids and Migration-sourced rows. Always emits history eventId
// from semanticEventKey when present. Omits optional decoded quest/objective text.
ContractExportResult ExportAccountStoreToContractV1(
    const AccountProgressStore& store,
    const ContractExportOptions& options);

} // namespace QuestProgress
