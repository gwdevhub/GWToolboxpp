#pragma once

// Runtime session identity binding helpers (Batch 2C).
// Pure key/metadata construction is offline-testable; live GWCA sampling lives in QuestProgressLive.cpp.

#include <Modules/QuestProgressJsonCodec.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace QuestProgress {

enum class IdentityKind : uint8_t {
    Unbound = 0,
    Ephemeral,  // missing/zero character UUID — memory only, never persist
    Persistent, // validated non-zero account + character UUIDs
};

struct SessionIdentity {
    IdentityKind kind = IdentityKind::Unbound;
    std::string account_key;     // normalized GUID; empty when unbound
    std::string character_uuid;  // normalized GUID; empty/zero when ephemeral/unbound
    std::string character_key;   // account_key + "/" + character_uuid when persistent
    std::string display_name;    // metadata only
    std::string profession;      // metadata only
    std::optional<bool> is_pre_searing;
};

using UuidWords = std::array<uint32_t, 4>;

bool IsZeroUuidWords(const UuidWords& words);
bool IsZeroUuidBytes(const uint8_t bytes[16]);

// Format 16 raw UUID bytes as lowercase GUID string (same layout as Windows GUID / GuidToString).
std::string FormatUuidBytes(const uint8_t bytes[16]);
std::string FormatUuidWords(const UuidWords& words);

// characterKey = accountKey + "/" + characterUuid (both already normalized).
std::string BuildCharacterKey(std::string_view normalized_account_key, std::string_view normalized_character_uuid);

// Build identity from already-owned UUID strings / metadata. No GWCA.
// Persistent only when both account and character normalize to non-empty non-zero GUIDs.
SessionIdentity MakeSessionIdentity(
    std::string_view account_uuid_raw,
    std::string_view character_uuid_raw,
    std::string_view display_name,
    std::string_view profession,
    std::optional<bool> is_pre_searing);

bool SamePersistentCharacter(const SessionIdentity& a, const SessionIdentity& b);
bool SameAccount(const SessionIdentity& a, const SessionIdentity& b);

} // namespace QuestProgress
