#pragma once

#include <string>
#include <vector>
#include <array>

#include <GWCA/Constants/Skills.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <Utils/TeamBuild.h>

namespace TeamBuildEncoder {

    // ---------------------------------------------------------------------------
    // Overview
    //
    // This namespace handles two distinct encoded representations of a hero team
    // build, plus the in-memory struct that both convert to/from:
    //
    //   TeamHeroBuild      Decoded in-memory representation (HeroBuildsWindow structs).
    //                      Holds up to 8 HeroBuild entries, each containing a
    //                      skill-template code string ("OwFj0..."), a HeroID, and
    //                      behaviour/panel settings.
    //
    //   DaybreakTeamBuild  GW's own party-loadout format: a narrow (char) string
    //                      using GW's base64 alphabet.  The game itself produces and
    //                      consumes these; the first character always encodes magic
    //                      byte 0x1F (header=15, type=1 packed LSB-first).
    //                      Example: "OwFj0dKEAAAAAAAA..."
    //
    //   EncodedTeamBuild   Our own compact wstring format designed to survive GW's
    //                      in-game chat/whisper transport within the 120-wchar link
    //                      budget.  Every 9 raw bits map to one wchar chosen from
    //                      a set of Unicode ranges that GW accepts in chat.
    //                      An 8-hero build fits in at most ~117 wchars.
    // ---------------------------------------------------------------------------

    using TeamHeroBuild     = ::TeamBuild;
    using DaybreakTeamBuild = std::string;
    using EncodedTeamBuild  = std::wstring;

    // ---------------------------------------------------------------------------
    // EncodedTeamBuild  –  compact wstring, safe for GW whisper transport
    // ---------------------------------------------------------------------------
    //
    // Bit-stream layout (LSB-first throughout):
    //
    //   [ 4 bits ] magic = 0xF   (identifies the format; checked by IsEncodedTeamBuild)
    //   [ 4 bits ] hero count N  (0–8)
    //
    //   For each of the N heroes:
    //     [ 6 bits ] HeroID
    //     [ 4 bits ] primary profession   (0–10)
    //     [ 4 bits ] secondary profession (0–10)
    //     [ 8 bits ] total attribute points spent (sum of kAttrCost[rank] over all attrs)
    //
    //     Attributes – written in the canonical order defined by kProfessionAttributes,
    //     primary profession first then secondary, stopping early when the remaining
    //     point budget reaches zero:
    //       [ BitsForAttrValue(remaining) bits ] rank for each attribute
    //       (BitsForAttrValue returns 4/3/2/0 bits depending on how many points remain)
    //
    //     Skills (8 slots, one per bar position):
    //       [ 9 bits ] 1-based index into the deterministic accessible-skill list
    //                  produced by GetAccessibleSkills(primary, secondary); 0 = empty slot.
    //                  The list is sorted and excludes PvP skills, so the index is
    //                  stable across clients as long as skill data is identical.
    //
    // wchar encoding:
    //   The raw byte buffer is re-packed 9 bits at a time.  Each 9-bit value (0–511)
    //   maps to a single wchar_t drawn from six GW-safe Unicode sub-ranges:
    //     val   0– 26  → U+0020–U+003A  (space … colon)
    //     val  27– 57  → U+003C–U+005A  (< = > ? @ A–Z)
    //     val  58– 58  → U+005C         (backslash)
    //     val  59– 91  → U+005E–U+007E  (^ _ ` a–z { | } ~)
    //     val  92–314  → U+00A1–U+017F  (Latin-1 Supplement + Latin Extended-A)
    //     val 315–511  → U+0391–U+0455  (Greek + Cyrillic)
    //   The characters '[', ']', and ';' are excluded so the string is safe inside
    //   a [label;encoded] chat link without escaping.
    //   512 values = exactly 2^9, so every 9-bit group maps cleanly to one wchar.
    // ---------------------------------------------------------------------------

    // Encode a TeamHeroBuild into an EncodedTeamBuild for whisper transport.
    // Returns empty on failure.
    EncodedTeamBuild  TeamBuildToEncoded(const TeamHeroBuild& tbuild);

    // Decode an EncodedTeamBuild back into a TeamHeroBuild.
    // Returns false on failure.
    bool              EncodedToTeamBuild(const EncodedTeamBuild& encoded, TeamHeroBuild& out);

    // Returns true if the wstring looks like one of our EncodedTeamBuilds.
    bool              IsEncodedTeamBuild(const EncodedTeamBuild& encoded);

    bool IsSkillEquippable(const GW::Constants::SkillID);

    // ---------------------------------------------------------------------------
    // DaybreakTeamBuild  –  GW's native base64 party-loadout format
    // ---------------------------------------------------------------------------
    //
    // Bit-stream layout (LSB-first throughout):
    //
    //   [ 4 bits ] header = 15
    //   [ 4 bits ] type   =  1  (party loadout; first byte is therefore 0x1F)
    //   [ 4 bits ] hero count N (0–8)
    //
    //   For each of the N heroes:
    //     [  8 bits ] HeroID
    //     [  2 bits ] prof_code  (0 if both professions fit in 4 bits, 1 if 6 bits needed)
    //       [ p bits ] primary profession    where p = prof_code*2 + 4  (4 or 6 bits)
    //       [ p bits ] secondary profession
    //     [  4 bits ] attribute count A  (0–12)
    //     [  4 bits ] attr_code  (0 if all attribute IDs fit in 4 bits, 2 if 6 bits needed)
    //       For each of the A attributes:
    //         [ a bits ] attribute ID     where a = attr_code + 4  (4 or 6 bits)
    //         [ 4 bits ] attribute rank   (0–12)
    //     [  4 bits ] skill_code  (0 if all skill IDs fit in 8 bits, 1 if 9 bits needed)
    //       For each of 8 skill slots:
    //         [ s bits ] SkillID           where s = skill_code + 8  (8 or 9 bits)
    //
    // Character encoding:
    //   The raw byte buffer is packed 6 bits at a time, LSB-first.  Each 6-bit value
    //   (0–63) is mapped to GW's own base64 alphabet:
    //     "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
    //   This is identical to standard RFC 4648 base64 and is the format the game
    //   uses natively for its in-game skill/build template codes.
    // ---------------------------------------------------------------------------

    // Encode a TeamHeroBuild into a DaybreakTeamBuild (GW base64 party loadout string).
    // Returns empty on failure.
    DaybreakTeamBuild TeamBuildToDaybreak(const TeamHeroBuild& tbuild);

    // Decode a DaybreakTeamBuild back into a TeamHeroBuild.
    // Returns false on failure.
    bool              DaybreakToTeamBuild(const DaybreakTeamBuild& daybreak, TeamHeroBuild& out);

    // Returns true if the string looks like a GW party loadout (magic byte 0x1F).
    bool              IsDaybreakTeamBuild(const DaybreakTeamBuild& daybreak);

    // ---------------------------------------------------------------------------
    // Chat helpers
    // ---------------------------------------------------------------------------

    // Format a TeamHeroBuild as a GW chat message with a clickable link.
    // Produces [name;EncodedTeamBuild] for use with ParseCustomLinks.
    std::wstring      ToChatMessage(const TeamHeroBuild& tbuild);

    // ---------------------------------------------------------------------------
    // Internals exposed for testing
    // ---------------------------------------------------------------------------

    // Attribute cost table: cost to reach rank N (1-indexed)
    constexpr int kAttrCost[] = { 0, 1, 3, 6, 10, 15, 21, 28, 37, 48, 61, 77, 97 };

    // Canonical attribute order per profession (index 0 = None)
    extern const std::array<GW::Constants::Attribute, 5> kProfessionAttributes[11];

    // Returns number of bits needed to encode an attribute value
    // given the remaining point budget.
    int BitsForAttrValue(int remaining_points);

    // Returns the deterministic sorted skill list accessible to a primary/secondary combo.
    // Excludes PvP skills. Includes PvE skills. Profession-matched only (no no-profession skills).
    std::vector<GW::Constants::SkillID> GetAccessibleSkills(
        GW::Constants::Profession primary,
        GW::Constants::Profession secondary);

} // namespace TeamBuildEncoder
