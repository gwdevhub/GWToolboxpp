#pragma once

#include <array>
#include <string>
#include <vector>
#include <ToolboxWindow.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Utilities/Hook.h>

#include <Windows/Splits/GoalClock.h>
#include <Windows/Splits/GoalEngine.h>
#include <Windows/Splits/GoalList.h>
#include <Windows/Splits/NuzlockeState.h>
#include <Windows/Splits/SplitsProfile.h>
#include <Windows/Splits/SplitsGoalListWindow.h>

inline constexpr int kProfileCount = 3;

// ---------------------------------------------------------------------------
// SplitsWindow — speedrun split timer, ported from the GWSplits plugin.
// ---------------------------------------------------------------------------
class SplitsWindow : public ToolboxWindow {
    // Out-of-line, matching NuzlockeState's own EncString-dependent dtor (see NuzlockeState.h).
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
    // Replaces the active list wholesale with an already-built one (e.g. from SCPresets); see the .cpp for why this isn't just NewActiveList() + mutating List()->goals after.
    void SetActiveList(GoalList list);
    // Cached — GetSavedLists() is a real directory_iterator disk scan and used to run unconditionally every ImGui frame the Splits settings section was open. See the cache fields below for invalidation.
    [[nodiscard]] std::vector<std::pair<std::string, std::wstring>> GetSavedLists() const;

    // Profile-specific subfolder paths for templates and run history.
    [[nodiscard]] std::wstring ActiveSplitsFolder() const;
    [[nodiscard]] std::wstring ActiveRunsFolder()   const;

    // Always the fastest completed run in history regardless of comparison_mode — UpdateReferenceIfPB() needs the real PB, not whatever's on screen.
    [[nodiscard]] const std::vector<double>& PBSplits()     const { return pb_splits_; }
    [[nodiscard]] const std::vector<double>& PBSplitsGame() const { return pb_splits_game_; }
    [[nodiscard]] double PBTotal()                          const { return pb_total_real_; }

    // Comparison data actually fed to the goal list's Ahead/Behind display — selects among PBSplits()/average/last-run per ActiveProfile().comparison_mode.
    [[nodiscard]] const std::vector<double>& CompareSplits()     const;
    [[nodiscard]] const std::vector<double>& CompareSplitsGame() const;

    [[nodiscard]] bool        HasPendingResume()  const { return pending_resume_; }
    [[nodiscard]] const char* PendingResumeName() const { return pending_resume_name_.c_str(); }
    void ApplyResume();
    void DiscardResume();

    // Nuzlocke: two independently-enabled modules, Manual profile only — Running's sequential zone-transition splits don't fit Nuzlocke. Settings checkboxes stay editable regardless of profile; only runtime behavior is Manual-gated, via these two accessors rather than the raw fields directly.
    void DrawNuzlockeSection();
    [[nodiscard]] bool NuzlockeDeathRulesEnabled() const { return nuzlocke_.death_rules_enabled && active_profile_idx_ == 0; }
    [[nodiscard]] bool NuzlockePointsEnabled() const { return nuzlocke_.points_enabled && active_profile_idx_ == 0; }
    // Sum of point values (Settings > Splits > Nuzlocke > Points) for every Completed, non-header goal in the active list.
    [[nodiscard]] int NuzlockeTotalPoints() const;

private:
    GoalClock             clock_;
    GoalEngine            engine_;
    GoalList              active_list_;
    SplitsGoalListWindow  window_;

    // GetSavedLists() cache. Invalidated whenever the requested folder differs from the cached one (handles profile switches) or cached_saved_lists_dirty_ is set (SaveActiveList()/NewActiveList()).
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
    // Set by the InstanceLoadFile bus event (file_id + DoA spawn point); consumed by ApplySCAutoLoadPreset() independently of pending_map_enter_ since packet order isn't guaranteed to match InstanceLoadInfo's tick.
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
    // Manual: "Time until mission start" ready-check dialog is up (Vizunah Square, Unwaking Waters, etc.) — game time shouldn't accumulate while waiting on other players here.
    bool     in_mission_queue_          = false;
    // User-initiated pause (Pause button / keybind, toggled via StartRun()); tracked separately from clock_.RealTime() since a manual pause freezes real time too.
    bool     manually_paused_           = false;
    double   manual_pause_accum_        = 0.0;
    double   total_paused_real_         = 0.0; // running total across the whole run; persisted with the run

    std::wstring splits_folder_;
    std::wstring runs_folder_;

    // Direct StoC/UIMessage hooks (registered/removed in Initialize()/Terminate()) — one per event type this window actually consumes.
    GW::HookEntry on_mission_complete_;
    GW::HookEntry on_objective_add_;
    GW::HookEntry on_vanquish_complete_;
    GW::HookEntry on_party_defeated_;
    GW::HookEntry on_objective_done_;
    GW::HookEntry on_objective_started_;
    GW::HookEntry on_door_;
    GW::HookEntry on_agent_allegiance_;
    GW::HookEntry on_doa_zone_;
    GW::HookEntry on_dungeon_reward_;
    GW::HookEntry on_server_message_;
    GW::HookEntry on_display_dialogue_;
    GW::HookEntry on_countdown_start_;
    GW::HookEntry on_skill_activate_;
    GW::HookEntry on_party_lock_;
    GW::HookEntry on_instance_load_info_;
    GW::HookEntry on_instance_load_file_;
    GW::HookEntry on_game_srv_transfer_;
    GW::HookEntry on_party_player_add_;
    GW::HookEntry on_party_player_remove_;
    GW::HookEntry on_party_hero_add_;
    GW::HookEntry on_party_hero_remove_;
    GW::HookEntry on_party_henchman_add_;
    GW::HookEntry on_party_henchman_remove_;
    GW::HookEntry on_agent_state_;
    GW::HookEntry on_quest_update_;
    GW::HookEntry on_quest_details_changed_;
    GW::HookEntry on_quest_remove_;

    void SaveResumeState();
    void DeleteResumeState();
    void SaveCompletedRun();
    void FailRun(const char* reason = "party defeated");
    void SaveRunToHistory(bool failed);
    // refresh_comparisons=false only rescans pb_splits_/pb_total_real_ (for UpdateReferenceIfPB's is-this-a-new-PB check), leaving avg_splits_/last_splits_ untouched so a just-finished run's "Run complete!" screen still compares against the PRE-run history rather than itself.
    void LoadPB(bool refresh_comparisons = true);
    void UpdateReferenceIfPB();
    // Shared by LoadActiveList/SetActiveList/ResetRun — resets every run-progress flag to its pre-run state (not the clock/engine, which each caller handles differently around this call).
    void ResetRunFlags();
    // Filesystem-safe "<active_list_.name>.json" path under ActiveRunsFolder() — shared by LoadPB (reads it) and SaveRunToHistory (reads+writes it).
    [[nodiscard]] std::wstring RunHistoryFilePath() const;

    // Timer policy: auto-fail conditions and Manual's per-goal-type auto-start rules. Deliberately separate from GoalEngine's fire/complete switch — trigger firing and clock policy are independent pieces, so some structural duplication between the two switches is accepted.
    void ApplyTimerPolicy(bool just_entered_map, int player_level);

    // SC only: swaps in the matching live-built preset when entering a dungeon/elite area with no run in progress. Runs before engine_.Update() each tick, never interrupts a run underway, and never overrides a non-preset (user-authored) list. DoA is handled first via pending_doa_file_id_/pending_doa_spawn_ since its zone rotation isn't map_id-driven.
    void ApplySCAutoLoadPreset(bool just_entered_map);

    bool        run_complete_   = false;
    bool        run_failed_     = false;
    std::string run_char_name_;
    int         run_char_level_ = 0;
    int64_t     run_start_unix_ = 0;

    std::vector<double> pb_splits_;
    std::vector<double> pb_splits_game_;
    double              pb_total_real_ = std::numeric_limits<double>::quiet_NaN();
    // Mean of every non-failed run's time at each goal index (a run reaching only goal 3 of 5 still contributes to goals 0-2). All three (pb/avg/last) recompute together in LoadPB().
    std::vector<double> avg_splits_;
    std::vector<double> avg_splits_game_;
    // Most recent run in history, failed or not — whatever splits it actually reached.
    std::vector<double> last_splits_;
    std::vector<double> last_splits_game_;

    float       resume_save_timer_   = 0.f;
    bool        pending_resume_      = false;
    std::string pending_resume_name_;
    std::string pending_resume_data_;

    // Debug log for party/quest/death/objective/preset events, surfaced in settings to validate hooks. Off by default, same idea as OT's show_debug_events but rendered as an in-UI scrollable list instead of Log::Info.
    bool debug_log_events_ = false;
    struct ChallengeDbgEvent {
        const char* tag;
        uint32_t    v1;
        uint32_t    v2;
    };
    std::vector<ChallengeDbgEvent> challenge_dbg_events_;
    // No-op unless debug_log_events_ is on. Appends one entry, capping at 200 (drops oldest) — single place for the enable-check/cap/erase logic so every call site stays consistent.
    void PushDbgEvent(const char* tag, uint32_t v1, uint32_t v2);

    // ---- Nuzlocke: Death Rules + Points state (see NuzlockeState.h) --------
    NuzlockeState nuzlocke_;

    void NuzlockeOnHeroAdd(uint32_t agent_id, GW::Constants::HeroID hero_id);
    void NuzlockeOnHeroRemove(uint32_t agent_id);
    void NuzlockeOnHenchAdd(uint32_t agent_id, const wchar_t* name_enc, size_t name_len);
    void NuzlockeOnHenchRemove(uint32_t agent_id);
    void NuzlockeOnAgentDied(uint32_t agent_id);
    void NuzlockeOnInstanceLoad();
    void NuzlockeUpdate(); // polls nuzlocke_.pending_hench_names; refreshes nuzlocke_.city_hench_available
    // Wipes death counts/rosters (called from ResetRun()) but keeps nuzlocke_.agents intact and immediately reseeds heroes/henches/self from it, so current-party tracking keeps working without waiting on the next zone transition.
    void NuzlockeResetProgress();
    // Canonical nuzlocke_.henches map key for a raw decoded hench name: the raw name itself under "campaign separation", or its bracket-stripped display name under "same character" merging. See nuzlocke_.merge_hench_by_name.
    [[nodiscard]] std::wstring NuzlockeHenchKey(const std::wstring& raw_name) const;

    static constexpr std::array<const char*, kProfileCount> kProfileSections = {
        "Splits.Manual", "Splits.Running", "Splits.SC"
    };
};
