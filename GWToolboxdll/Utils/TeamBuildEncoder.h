#pragma once

#include <string>
#include <vector>
#include <array>

#include <GWCA/Constants/Skills.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <Utils/TeamBuild.h>

namespace TeamBuildEncoder {

    using TeamHeroBuild     = ::TeamBuild;
    using DaybreakTeamBuild = std::string;
    using EncodedTeamBuild  = std::wstring;

    // Encode a TeamHeroBuild into an EncodedTeamBuild for whisper transport.
    // Returns empty on failure.
    EncodedTeamBuild  TeamBuildToEncoded(const TeamHeroBuild& tbuild);

    // Decode an EncodedTeamBuild back into a TeamHeroBuild.
    // Returns false on failure.
    bool              EncodedToTeamBuild(const EncodedTeamBuild& encoded, TeamHeroBuild& out);

    // Returns true if the wstring looks like one of our EncodedTeamBuilds.
    bool              IsEncodedTeamBuild(const EncodedTeamBuild& encoded);

    bool IsSkillEquippable(const GW::Constants::SkillID);

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
