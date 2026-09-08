#pragma once

// Internal quest-progress JSON codec (not Contract v1 export). Uses glaze.

#include <Modules/QuestProgressDomain.h>
#include <Modules/QuestCharacterJourney.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace QuestProgress {

inline constexpr char kStoreFormatId[] = "gwtoolbox-quest-progress";
inline constexpr uint32_t kStoreFormatMajor = 1;
// Minor 1: identity migration from 1.0 (normalizes ordering / defaults). Current write version.
inline constexpr uint32_t kStoreFormatMinor = 1;

struct StoreVersion {
    uint32_t major = kStoreFormatMajor;
    uint32_t minor = kStoreFormatMinor;
};

struct MissionRecord {
    uint32_t map_id = 0;
    bool completed_normal = false;
    bool completed_hard = false;
    bool bonus_normal = false;
    bool bonus_hard = false;
    std::string last_observed_at;
};

struct StoredCharacter {
    std::string character_key;
    std::string display_name;
    std::string profession;
    std::string secondary_profession;
    std::optional<bool> is_pre_searing;
    std::string first_observed_at;
    std::string last_observed_at;
    std::map<uint32_t, QuestProgress> quests;
    std::map<uint32_t, MissionRecord> missions;
    std::map<uint32_t, TitleStateRecord> titles;
    std::optional<uint32_t> last_known_level;
    std::optional<uint32_t> last_map_id;
    std::vector<JourneyEventRecord> journey_events;
};

struct AccountProgressStore {
    std::string store_format = kStoreFormatId;
    StoreVersion store_version{};
    std::string account_key;
    std::map<std::string, StoredCharacter> characters;
};

enum class CodecStatus : uint8_t {
    Ok = 0,
    EmptyStore,
    ParseError,
    ValidationError,
    UnsupportedNewerMajor,
    AccountKeyMismatch,
    MigrationError,
};

struct CodecDiagnostics {
    std::vector<std::string> messages;
    bool migrated = false;
    bool recovered_from_bak = false;
};

struct CodecParseResult {
    CodecStatus status = CodecStatus::ParseError;
    AccountProgressStore store;
    CodecDiagnostics diagnostics;
};

struct CodecSerializeResult {
    CodecStatus status = CodecStatus::ParseError;
    std::string utf8_json;
    CodecDiagnostics diagnostics;
};

// Normalize GUID-like account keys to lowercase "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx".
// Returns empty string if invalid.
std::string NormalizeAccountKey(std::string_view raw);

bool IsValidNormalizedAccountKey(std::string_view key);

// Parse internal store JSON. expected_account_key must match file accountKey when non-empty in JSON.
CodecParseResult ParseAccountStoreJson(
    std::string_view utf8_json,
    std::string_view expected_account_key);

// Deterministic UTF-8 JSON (prettified, stable field/array order).
CodecSerializeResult SerializeAccountStoreJson(const AccountProgressStore& store);

// Explicit migration to current minor within major 1.
bool MigrateAccountStoreToCurrent(AccountProgressStore& store, CodecDiagnostics& diagnostics);

// Sort maps/vectors for deterministic serialization (also used after merge).
void CanonicalizeAccountStore(AccountProgressStore& store);

// LE hex encode/decode for objective encoded content (portable file form).
std::string EncodeContentToLeHex(std::u16string_view encoded);
bool DecodeContentFromLeHex(std::string_view hex, std::u16string& out);

} // namespace QuestProgress
