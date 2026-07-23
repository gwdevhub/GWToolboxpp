#pragma once

#include <array>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <ToolboxWindow.h>
#include <GWCA/GameContainers/GamePos.h>

#include <Windows/Splits/GoalClock.h>
#include <Windows/Splits/GoalEngine.h>
#include <Windows/Splits/GoalList.h>
#include <Windows/Splits/SplitsProfile.h>
#include <Windows/Splits/SplitsGoalListWindow.h>

namespace GuiUtils {
    class EncString;
}
namespace GW {
    namespace Constants {
        enum HeroID : uint32_t;
    }
}

inline constexpr int kProfileCount = 3;

// ---------------------------------------------------------------------------
// SplitsWindow — speedrun split timer, ported from the GWSplits plugin.
// ---------------------------------------------------------------------------
class SplitsWindow : public ToolboxWindow {
    // Out-of-line: nuzlocke_pending_hench_names_ holds unique_ptr<GuiUtils::EncString>,
    // which is only forward-declared here. The constructor needs EncString complete to
    // generate member-unwind cleanup, and the destructor to generate the actual delete —
    // both must be instantiated in the .cpp where EncString.h is included, not implicitly
    // wherever Instance() is called (its inline body re-emits both in every calling TU).
    SplitsWindow();
    ~SplitsWindow() override;

public:
    static SplitsWindow& Instance()
    {
        static SplitsWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Splits"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_STOPWATCH; }

    void Initialize() override;
    void Terminate() override;

    void Update(float delta) override;
    void Draw(IDirect3DDevice9* device) override;
    void DrawSettingsInternal() override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;

    // Called by UI
    void StartRun();
    void ResetRun();
    void TriggerManualSplit();
    // Switches active profile, resets run state, reloads any bound list.
    void SwitchProfile(int idx);

    // Keybind accessors (VK codes; 0 = unbound)
    int& KeyStart() { return key_start_; }
    int& KeyReset() { return key_reset_; }
    int& KeySplit() { return key_split_; }

    // Active profile accessor (read/write for UI)
    [[nodiscard]] SplitsProfile&       ActiveProfile()       { return profiles_[active_profile_idx_]; }
    [[nodiscard]] const SplitsProfile& ActiveProfile() const { return profiles_[active_profile_idx_]; }
    [[nodiscard]] int                  ActiveProfileIdx()  const { return active_profile_idx_; }
    [[nodiscard]] std::array<SplitsProfile, kProfileCount>& Profiles() { return profiles_; }

    // Read-only accessors for UI
    [[nodiscard]] const GoalClock&     Clock()       const { return clock_; }
    [[nodiscard]] const GoalEngine&    Engine()      const { return engine_; }
    [[nodiscard]] GoalList*            List()              { return &active_list_; }
    [[nodiscard]] GW::Constants::MapID CurrentMap()  const { return last_map_; }
    [[nodiscard]] bool RunComplete()                 const { return run_complete_; }
    [[nodiscard]] bool RunFailed()                   const { return run_failed_; }
    // Includes the in-progress pause so the display ticks up live while paused, not just on resume.
    [[nodiscard]] double TotalPausedReal()           const {
        return total_paused_real_ + (manually_paused_ ? manual_pause_accum_ : 0.0);
    }

    void NewActiveList(const char* name);
    void SaveActiveList();
    void LoadActiveList(const std::wstring& path);
    // Replaces the active list wholesale with an already-built one (e.g. from SCPresets) —
    // see the .cpp for why this isn't just NewActiveList() + mutating List()->goals after.
    void SetActiveList(GoalList list);
    // Cached — GoalList::ListSaved() is a real directory_iterator disk scan, and this used to
    // be called unconditionally from DrawSettings() every single ImGui frame the Splits
    // settings section (or the list-picker combo) was open. See the cache fields (private,
    // below) for what invalidates it.
    [[nodiscard]] std::vector<std::pair<std::string, std::wstring>> GetSavedLists() const;

    // Profile-specific subfolder paths for templates and run history.
    [[nodiscard]] std::wstring ActiveSplitsFolder() const;
    [[nodiscard]] std::wstring ActiveRunsFolder()   const;

    // Always the fastest completed run in history, regardless of the active profile's display
    // comparison_mode — UpdateReferenceIfPB() needs the real PB, not whatever's on screen.
    [[nodiscard]] const std::vector<double>& PBSplits()     const { return pb_splits_; }
    [[nodiscard]] const std::vector<double>& PBSplitsGame() const { return pb_splits_game_; }
    [[nodiscard]] double PBTotal()                          const { return pb_total_real_; }

    // Comparison data actually fed to the goal list's Ahead/Behind delta display — selects
    // among PBSplits()/average/last-run per ActiveProfile().comparison_mode.
    [[nodiscard]] const std::vector<double>& CompareSplits()     const;
    [[nodiscard]] const std::vector<double>& CompareSplitsGame() const;

    [[nodiscard]] bool        HasPendingResume()  const { return pending_resume_; }
    [[nodiscard]] const char* PendingResumeName() const { return pending_resume_name_.c_str(); }
    void ApplyResume();
    void DiscardResume();

    // Nuzlocke: two independently-enabled modules, Manual profile only (idx 0) — Running
    // is a different playstyle (sequential zone-transition splits) that Nuzlocke doesn't
    // make sense for. Death Rules draws its roster/lives section below the goal list
    // (DrawNuzlockeSection); Points has no section of its own — its running total is drawn
    // left-aligned in the header clock row instead. The settings checkboxes themselves stay
    // editable regardless of active profile; only the runtime behavior is Manual-gated, via
    // these two accessors — every internal check goes through them rather than the raw
    // nuzlocke_death_rules_enabled_/nuzlocke_points_enabled_ fields.
    void DrawNuzlockeSection();
    [[nodiscard]] bool NuzlockeDeathRulesEnabled() const { return nuzlocke_death_rules_enabled_ && active_profile_idx_ == 0; }
    [[nodiscard]] bool NuzlockePointsEnabled() const { return nuzlocke_points_enabled_ && active_profile_idx_ == 0; }
    // Sum of point values (Settings > Splits > Nuzlocke > Points) for every Completed,
    // non-header goal in the active list.
    [[nodiscard]] int NuzlockeTotalPoints() const;

private:
    GoalClock             clock_;
    GoalEngine            engine_;
    GoalList              active_list_;
    SplitsGoalListWindow  window_;

    // GetSavedLists() cache — see its own comment. Invalidated (rescanned) whenever the
    // requested folder differs from the cached one (handles profile switches automatically)
    // or cached_saved_lists_dirty_ is set (SaveActiveList()/NewActiveList()).
    mutable std::vector<std::pair<std::string, std::wstring>> cached_saved_lists_;
    mutable std::wstring                                      cached_saved_lists_folder_;
    mutable bool                                              cached_saved_lists_dirty_ = true;

    // Profiles: 0=Manual, 1=Running, 2=SC
    std::array<SplitsProfile, kProfileCount> profiles_ = {
        MakeManualProfile(), MakeRunningProfile(), MakeSCProfile()
    };
    int active_profile_idx_ = 0;

    GW::Constants::MapID  last_map_                 = GW::Constants::MapID::None;
    bool                  last_was_explorable_      = false;
    // Set by the InstanceLoadInfo bus event; consumed once per Update() tick.
    bool                  pending_map_enter_        = false;
    bool                  pending_came_from_explorable_ = false;
    // Set by the PartyDefeated bus event; consumed by ApplyTimerPolicy().
    bool                  pending_party_defeated_   = false;
    // Set by the InstanceLoadFile bus event (file_id + DoA spawn point); consumed by
    // ApplySCAutoLoadPreset() independently of just_entered_map/pending_map_enter_ — server
    // packet order isn't guaranteed, so InstanceLoadFile can land on a different Update() tick
    // than the InstanceLoadInfo that drives the usual map-based path.
    uint32_t              pending_doa_file_id_      = 0;
    GW::Vec2f             pending_doa_spawn_        = {};

    int  key_start_ = 0;
    int  key_reset_ = 0;
    int  key_split_ = 0;
    bool key_start_prev_ = false;
    bool key_reset_prev_ = false;
    bool key_split_prev_ = false;

    uint32_t pending_skill_id_          = 0;     // skill fired by local player this tick (shadow step bus)
    bool     running_awaiting_movement_ = false; // armed after entering first Running goal's map
    bool     running_load_paused_       = false; // clock was paused by us on load-start; resume on player-struct ready
    // Manual: "Time until mission start" ready-check dialog is up (Vizunah Square, Unwaking
    // Waters, etc.) — game time shouldn't accumulate while waiting on other players here.
    bool     in_mission_queue_          = false;
    // User-initiated pause (Pause button / keybind, toggled via StartRun()). Tracked
    // separately from clock_.RealTime() since a manual pause freezes real time too.
    bool     manually_paused_           = false;
    double   manual_pause_accum_        = 0.0;
    double   total_paused_real_         = 0.0; // running total across the whole run; persisted with the run

    std::wstring splits_folder_;
    std::wstring runs_folder_;

    void SaveResumeState();
    void DeleteResumeState();
    void SaveCompletedRun();
    void FailRun(const char* reason = "party defeated");
    void SaveRunToHistory(bool failed);
    // refresh_comparisons=false only rescans pb_splits_/pb_total_real_ (needed for the
    // is-this-a-new-PB check in UpdateReferenceIfPB) and leaves avg_splits_/last_splits_
    // untouched — used right after a run finishes, so its own "Run complete!" screen still
    // compares against the history as it stood BEFORE this run (i.e. the previous attempt),
    // not against itself. Full refresh happens instead at the next run's actual start.
    void LoadPB(bool refresh_comparisons = true);
    void UpdateReferenceIfPB();

    // Timer policy: auto-fail conditions (party wipe, incomplete rezone) and the Manual
    // profile's per-goal-type auto-start rules. Deliberately separate from GoalEngine's
    // fire/complete switch (GoalEngine.cpp) — Splits (trigger firing) and Timer (clock
    // policy) are two independent pieces, so some structural duplication between the two
    // switches is expected/accepted rather than coupling them together.
    void ApplyTimerPolicy(bool just_entered_map, int player_level);

    // SC only: swaps in the matching preset (built live — see SCPresets::BuildPresetForMap) when
    // entering a dungeon/elite area with no run in progress. Runs before engine_.Update() each
    // tick so a freshly-swapped list is already attached in time for this same tick's
    // autostart/Pass-1 checks. Never interrupts a run already underway, and never overrides a
    // list that isn't itself a preset (GoalList::is_preset) — a genuine user-authored/saved
    // list always wins over auto-detection, matching "user-selected always wins."
    // Domain of Anguish is handled first, inside this same function, via the independently-
    // latched pending_doa_file_id_/pending_doa_spawn_ (see SCPresets::BuildDoAPresetList) —
    // its zone rotation isn't map_id-driven so it can't go through BuildPresetForMap below.
    void ApplySCAutoLoadPreset(bool just_entered_map);

    bool        run_complete_   = false;
    bool        run_failed_     = false;
    std::string run_char_name_;
    int         run_char_level_ = 0;
    int64_t     run_start_unix_ = 0;

    std::vector<double> pb_splits_;
    std::vector<double> pb_splits_game_;
    double              pb_total_real_ = std::numeric_limits<double>::quiet_NaN();
    // Mean of every non-failed run's time at each goal index (a run that only reached goal 3
    // of 5 still contributes to goals 0-2's average). All three (pb/avg/last) are recomputed
    // together in LoadPB() whenever run history reloads.
    std::vector<double> avg_splits_;
    std::vector<double> avg_splits_game_;
    // Most recent run in history, failed or not — whatever splits it actually reached.
    std::vector<double> last_splits_;
    std::vector<double> last_splits_game_;

    float       resume_save_timer_   = 0.f;
    bool        pending_resume_      = false;
    std::string pending_resume_name_;
    std::string pending_resume_data_;

    // Debug log for party/quest/death/objective/preset events — surfaced in settings to
    // validate hooks. Off by default (persisted) — same idea as OT's own "Debug: log events"
    // checkbox (ObjectiveTimerWindow::show_debug_events), just rendered as an in-UI scrollable
    // list here instead of routed through Log::Info.
    bool debug_log_events_ = false;
    struct ChallengeDbgEvent {
        const char* tag;
        uint32_t    v1;
        uint32_t    v2;
    };
    std::vector<ChallengeDbgEvent> challenge_dbg_events_;
    // No-op unless debug_log_events_ is on. Appends one entry, capping at 200 (drops oldest).
    // Single place for the enable-check/cap/erase logic so every call site (including future
    // ones) stays consistent.
    void PushDbgEvent(const char* tag, uint32_t v1, uint32_t v2);

    // ---- Nuzlocke: Death Rules (party death tracking) ----------------------
    // v1: roster is built lazily from whichever heroes/henchmen we've actually seen
    // this session; no pre-seeded campaign roster. Deaths only count in explorables.
    struct NuzlockeMember {
        std::wstring            name;
        int                     deaths     = 0;
        // Heroes and henchmen only (real players don't get an icon). Read once from the
        // agent at NuzlockeOnHenchAdd/NuzlockeOnHeroAdd time, since henchman names decode
        // async but the agent's profession byte is available immediately either way.
        GW::Constants::Profession profession = GW::Constants::Profession::None;
    };
    struct NuzlockeIdentity {
        bool                      is_hero = false;
        GW::Constants::HeroID     hero_id{};
        std::wstring              hench_name;
        GW::Constants::Profession hench_profession = GW::Constants::Profession::None;
    };

    bool nuzlocke_death_rules_enabled_ = false;
    int  nuzlocke_hero_lives_          = 1;
    int  nuzlocke_hench_lives_         = 1;
    bool nuzlocke_track_self_          = false;
    bool nuzlocke_track_other_players_ = false;
    int  nuzlocke_player_lives_        = 1;
    // Same real-world henchman name (e.g. "Eve") can be a different NPC/build with a
    // different bracket role-text in a different campaign or outpost. Off (default) keeps
    // each raw name+role distinct ("campaign separation"); on, they're folded into one
    // tracked entry by display name alone ("same character").
    bool nuzlocke_merge_hench_by_name_ = false;

    std::map<GW::Constants::HeroID, NuzlockeMember> nuzlocke_heroes_;
    std::map<std::wstring, NuzlockeMember>          nuzlocke_henches_;
    // Real players (self and/or others), keyed by character name. Resolved directly from
    // the agent at time of death (no pre-registration via PartyPlayerAdd needed — player
    // names aren't encoded/localized like hero names, so no async decode either).
    std::map<std::wstring, NuzlockeMember>          nuzlocke_players_;
    // agent_id -> identity, only while that agent is actually in the party.
    std::unordered_map<uint32_t, NuzlockeIdentity>  nuzlocke_agents_;
    // agent_ids already counted as dead — guards against AgentState re-firing the
    // dead bit more than once for the same death. Cleared on new instance load.
    std::unordered_set<uint32_t>                    nuzlocke_dead_agents_;
    // Henchman names decode asynchronously; polled from Update() until ready.
    std::vector<std::pair<uint32_t, std::unique_ptr<GuiUtils::EncString>>> nuzlocke_pending_hench_names_;
    // Hireable henchmen in the current outpost (WorldContext::henchmen_agent_ids), keyed by
    // agent_id so we don't re-issue a decode request for one already resolved. Cleared on
    // every instance load since agent_ids aren't stable/unique across instances.
    std::unordered_map<uint32_t, std::unique_ptr<GuiUtils::EncString>> nuzlocke_city_hench_names_;
    // Stripped display-names (bracket removed) of henchmen hireable in THIS outpost right
    // now — recomputed in NuzlockeUpdate() from WorldContext::henchmen_agent_ids, but only
    // when the id list changes or something's still unresolved (nuzlocke_town_hench_all_resolved_
    // below) rather than unconditionally every tick. Empty whenever in an explorable, since the
    // hireable roster is a town-only concept.
    std::unordered_set<std::wstring> nuzlocke_city_hench_available_;
    // Skip-check for the above: last frame's raw henchmen_agent_ids (order-sensitive compare
    // is fine — the array's own order is stable within one town visit) and whether every one of
    // them had a resolved profession icon that pass. Never skip while anything's unresolved, so
    // a hireable henchman's icon keeps retrying every tick until AgentLiving::primary actually
    // populates (can take a frame or more after first becoming visible) instead of getting stuck
    // blank the moment this skip kicks in. Reset on instance load since agent_ids aren't stable
    // across instances (a reused id must not compare as "same roster, already resolved").
    std::vector<uint32_t> nuzlocke_last_town_hench_ids_;
    bool                  nuzlocke_town_hench_all_resolved_ = false;

    void NuzlockeOnHeroAdd(uint32_t agent_id, GW::Constants::HeroID hero_id);
    void NuzlockeOnHeroRemove(uint32_t agent_id);
    void NuzlockeOnHenchAdd(uint32_t agent_id, const wchar_t* name_enc, size_t name_len);
    void NuzlockeOnHenchRemove(uint32_t agent_id);
    void NuzlockeOnAgentDied(uint32_t agent_id);
    void NuzlockeOnInstanceLoad();
    void NuzlockeUpdate(); // polls nuzlocke_pending_hench_names_; refreshes nuzlocke_city_hench_available_
    // Wipes death counts/rosters (called from ResetRun()) but keeps nuzlocke_agents_ intact
    // and immediately reseeds heroes/henches/self from it, so tracking for whoever's
    // currently in the party keeps working without waiting on the next zone transition.
    void NuzlockeResetProgress();
    // Canonical nuzlocke_henches_ map key for a raw decoded hench name — the raw name
    // itself under "campaign separation", or its bracket-stripped display name under
    // "same character" merging. See nuzlocke_merge_hench_by_name_.
    [[nodiscard]] std::wstring NuzlockeHenchKey(const std::wstring& raw_name) const;

    // ---- Nuzlocke: Points ---------------------------------------------------
    bool nuzlocke_points_enabled_ = false;
    // Point value per goal category, applied when a goal of that category completes.
    // Global (not per-list) — a list that isn't a Nuzlocke challenge just scores 0 since
    // these all default to 0. Field names/grouping mirror the Add Goal trigger dropdown.
    struct NuzlockePointValues {
        int manual       = 0;
        int missions     = 0; // MissionComplete + MissionBonus
        int explorables  = 0; // MapEnter
        int towns        = 0; // EnterExplorable, ExitExplorable, ExitOutpost
        int titles       = 0; // ReachTitleRank
        int reach_level  = 0;
        int quest        = 0; // QuestPickup + QuestComplete
        int skill_learnt = 0;
    };
    NuzlockePointValues nuzlocke_goal_points_;

    static constexpr std::array<const char*, kProfileCount> kProfileSections = {
        "Splits.Manual", "Splits.Running", "Splits.SC"
    };
};
