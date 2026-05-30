#pragma once

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/SkillbarMgr.h>

struct IDirect3DTexture9;

// Unified build entry used by both BuildsWindow and HeroBuildsWindow.
//   hero_id == NoHero  →  player build: shows pcons; hides hero controls.
//   hero_id != NoHero  →  hero build:   shows hero controls; hides pcons.
struct Build {
    Build() = default;
    Build(std::string_view name, std::string_view code,
          GW::Constants::HeroID hero_id = GW::Constants::HeroID::NoHero,
          int show_panel = 0, uint32_t behavior = 1, uint8_t disabled_skills = 0);

    std::string name{};
    std::string code{};
    GW::Constants::HeroID hero_id = GW::Constants::HeroID::NoHero;
    uint32_t behavior = 1;
    bool show_panel = false;
    uint8_t disabled_skills = 0;
    std::set<std::string> pcons{};

    bool IsPlayerBuild() const { return hero_id == GW::Constants::HeroID::NoHero; }

    // Returns the elite skill name from the build code as a fallback display name.
    std::string GetFallbackBuildName() const;

    // Decode skill template from code. Returns nullptr if code is invalid.
    const GW::SkillbarMgr::SkillTemplate* Decode();
    bool IsDecoded() const;
    const GW::Constants::SkillID* Skills();

    // Invalidate the cached decode result (call when code changes).
    void ResetDecodeCache();

    void View() const;
    void Send() const;
    void Load() const;
    void Copy() const;

    // Called every frame from GWToolbox::Update(); processes pending loads and the send queue.
    static void Update();
    // Push a pre-formatted chat string onto the rate-limited send queue.
    static void EnqueueSend(std::string msg);

private:
    GW::SkillbarMgr::SkillTemplate skill_template_{};
};

// Unified team build used by both BuildsWindow and HeroBuildsWindow.
//
//   has_hero_slots == false  →  BuildsWindow layout
//     · variable number of player builds
//     · pcons per build, show_numbers flag
//
//   has_hero_slots == true   →  HeroBuildsWindow layout
//     · builds[0] is the player slot, builds[1..N] are hero slots
//     · group / mode fields; hero selector / behavior / panel per hero build
struct TeamBuild {
    static uint32_t s_cur_ui_id;

    TeamBuild() = default;
    explicit TeamBuild(std::string_view name, std::string_view ui_id = {});
    TeamBuild(const TeamBuild& other);
    TeamBuild& operator=(const TeamBuild& other);

    bool edit_open = false;
    bool focus_next_frame = false;
    int mode = 0;              // 0 = don't change, 1 = normal, 2 = hard (hero builds)
    bool show_numbers = false; // prefix build names with index (player builds layout)
    bool has_hero_slots = false;
    std::string name{};
    std::string group{};
    std::string ui_id{};
    std::string pending_reroll_build_code{}; // set while waiting for a reroll to complete before loading
    std::vector<Build> builds{};

    // Callback signature: (teambuild, build_index)
    using BuildAction = std::function<void(TeamBuild&, size_t)>;

    // Draw the full ImGui edit window for this teambuild and its builds.
    //
    //   index        – position of this TeamBuild in all_builds.
    //   all_builds   – the owning vector; may be modified (reorder / delete).
    //   builds_changed – set true when any data is modified.
    //
    // Returns false when this teambuild was deleted (erased from all_builds).
    bool DrawEditWindow(
        size_t index,
        std::vector<TeamBuild>& all_builds,
        bool& builds_changed
    );

    // Draw a read-only detached window for a teambuild received via chat link.
    // hero_builds / builds_changed are the HeroBuildsWindow-owned list used by
    // the "Add to My Builds" button.
    void DrawDetachedWindow(std::vector<TeamBuild>& hero_builds, bool& builds_changed);

    // Provide the 2×2 sprite sheet used to render the disabled-skill overlay.
    // Call once from HeroBuildsWindow::Initialize().
    static void SetSkillToggleSprite(IDirect3DTexture9** sprite);

    void Send(bool one_by_one = false) const;
    void Load() const;
    void Copy() const;
    TeamBuild Duplicate();

    // Draw tooltip content showing each build's skill bar, or a red "No Build Defined"
    // placeholder when the build code is absent or invalid.
    void DrawTooltip() const;

    // Returns true if [TB;<encoded>] would be >= 120 chars (GW chat limit).
    bool ChatCodeTooLong() const;

    // Invalidate the encoded teambuild cache (call when build codes or hero IDs change).
    void ResetEncodedCache() const;

private:
    int editing_build_idx_ = -1; // which build row is expanded (player-builds layout)
    bool send_all_confirming_ = false;
    mutable std::optional<std::wstring> encoded_cache_{};

    // Returns the encoded wstring, computing and caching it on first call.
    const std::wstring& GetEncoded() const;

    void DrawPlayerBuildsContent(bool& builds_changed);

    void DrawHeroBuildsContent(bool& builds_changed);
};
