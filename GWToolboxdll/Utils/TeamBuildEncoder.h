#pragma once

#include <string>
#include <vector>
#include <array>

#include <GWCA/Constants/Skills.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/SkillbarMgr.h>

// Forward declarations
struct HeroBuild {
    HeroBuild() = default;
    HeroBuild(std::string_view n, std::string_view c, GW::Constants::HeroID _hero_id = GW::Constants::HeroID::NoHero, int panel = 0, uint32_t _behavior = 1, uint8_t _disabled_skills = 0)
        : name(n), code(c), hero_id(_hero_id), behavior(_behavior), show_panel(panel), disabled_skills(_disabled_skills)
    {
        if (name.empty()) name = GetFallbackBuildName();
    }
    std::string name{};
    std::string code{};
    GW::Constants::HeroID hero_id = GW::Constants::HeroID::NoHero;
    uint32_t behavior = 1;
    bool show_panel = false;
    uint8_t disabled_skills = 0;
    std::string GetFallbackBuildName() const;
};

struct TeamHeroBuild {
    static unsigned int cur_ui_id;
    TeamHeroBuild(std::string_view n, std::string_view _ui_id = {}) : name(n), ui_id(_ui_id.empty() ? std::to_string(++cur_ui_id) : std::string(_ui_id)) {}
    bool edit_open = false;
    bool focus_next_frame = false;
    int mode = 0;
    std::string name{};
    std::string group{};
    std::string ui_id{};
    std::array<HeroBuild, 8> builds{};
};

namespace TeamBuildEncoder {

    // ---------------------------------------------------------------------------
    // Types
    //
    // TeamHeroBuild      - decoded in-memory representation (HeroBuildsWindow structs)
    // DaybreakTeamBuild  - GW's native base64 party loadout string e.g. "OwFj0..."
    // EncodedTeamBuild   - our compressed wstring, safe for GW whisper transport
    // ---------------------------------------------------------------------------

    using TeamHeroBuild     = TeamHeroBuild;
    using DaybreakTeamBuild = std::string;
    using EncodedTeamBuild  = std::wstring;

    // ---------------------------------------------------------------------------
    // EncodedTeamBuild: our compressed format (9 bits/wchar, <122 wchars worst case)
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
    // DaybreakTeamBuild: GW's native base64 party loadout format
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
