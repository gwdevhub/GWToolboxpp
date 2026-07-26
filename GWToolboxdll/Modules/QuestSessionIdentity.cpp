#include <Modules/QuestSessionIdentity.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <cstdio>
#include <cstring>

namespace QuestProgress {
namespace {

bool AllZero(const uint8_t* p, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        if (p[i] != 0) {
            return false;
        }
    }
    return true;
}

} // namespace

bool IsZeroUuidBytes(const uint8_t bytes[16])
{
    return AllZero(bytes, 16);
}

bool IsZeroUuidWords(const UuidWords& words)
{
    return words[0] == 0 && words[1] == 0 && words[2] == 0 && words[3] == 0;
}

std::string FormatUuidBytes(const uint8_t bytes[16])
{
    // Interpret as Windows GUID memory layout.
    char out[37];
    const auto* g = reinterpret_cast<const GUID*>(bytes);
    std::snprintf(
        out, sizeof(out),
        "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        g->Data1, g->Data2, g->Data3,
        g->Data4[0], g->Data4[1], g->Data4[2],
        g->Data4[3], g->Data4[4], g->Data4[5],
        g->Data4[6], g->Data4[7]);
    return NormalizeAccountKey(out);
}

std::string FormatUuidWords(const UuidWords& words)
{
    uint8_t bytes[16];
    std::memcpy(bytes, words.data(), 16);
    return FormatUuidBytes(bytes);
}

std::string BuildCharacterKey(std::string_view normalized_account_key, std::string_view normalized_character_uuid)
{
    std::string out;
    out.reserve(normalized_account_key.size() + 1 + normalized_character_uuid.size());
    out.append(normalized_account_key);
    out.push_back('/');
    out.append(normalized_character_uuid);
    return out;
}

SessionIdentity MakeSessionIdentity(
    std::string_view account_uuid_raw,
    std::string_view character_uuid_raw,
    std::string_view display_name,
    std::string_view profession,
    std::optional<bool> is_pre_searing)
{
    SessionIdentity id;
    id.display_name = std::string(display_name);
    id.profession = std::string(profession);
    id.is_pre_searing = is_pre_searing;

    const auto account = NormalizeAccountKey(account_uuid_raw);
    const auto character = NormalizeAccountKey(character_uuid_raw);
    if (account.empty()) {
        id.kind = IdentityKind::Unbound;
        return id;
    }
    id.account_key = account;

    if (character.empty() || character == "00000000-0000-0000-0000-000000000000") {
        id.kind = IdentityKind::Ephemeral;
        id.character_uuid.clear();
        id.character_key.clear();
        return id;
    }

    id.kind = IdentityKind::Persistent;
    id.character_uuid = character;
    id.character_key = BuildCharacterKey(account, character);
    return id;
}

bool SamePersistentCharacter(const SessionIdentity& a, const SessionIdentity& b)
{
    return a.kind == IdentityKind::Persistent
        && b.kind == IdentityKind::Persistent
        && a.character_key == b.character_key
        && a.account_key == b.account_key;
}

bool SameAccount(const SessionIdentity& a, const SessionIdentity& b)
{
    return !a.account_key.empty() && a.account_key == b.account_key;
}

} // namespace QuestProgress
