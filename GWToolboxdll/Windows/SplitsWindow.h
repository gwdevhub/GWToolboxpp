#pragma once

#include <array>
#include <functional>
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

// Plain (non-optional) fields since this is UI-facing, not the JSON DTO (SplitsWindowJson::SerializedRun) it's built from.
struct RecentRunGoal {
    std::string label;
    double      real_time = 0.0;
    double      game_time = 0.0;
    bool        completed = false;
};
struct RecentRun {
    double                     total_real = 0.0;
    bool                       failed     = false;
    int64_t                    utc_start  = 0;
    std::vector<RecentRunGoal> goals;
};

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

    // Colors — shared across all profiles rather than per-profile; not enough value in letting Manual/Running/SC look different to justify tripling the config.
    Color& ColorCompleted() { return color_completed_; }
    Color& ColorActive()    { return color_active_; }
    Color& ColorRealTime()  { return color_real_time_; }
    Color& ColorGameTime()  { return color_game_time_; }
    Color& ColorPbAhead()   { return color_pb_ahead_; }
    Color& ColorPbBehind()  { return color_pb_behind_; }

    // Active profile accessor (read/write for UI)
    [[nodiscard]] SplitsProfile&       ActiveProfile()       { return profiles_[active_profile_idx_]; }
    [[nodiscard]] const SplitsProfile& ActiveProfile() const { return profiles_[active_profile_idx_]; }
    [[nodiscard]] int                  ActiveProfileIdx()  const { return active_profile_idx_; }
    [[nodiscard]] std::array<SplitsProfile, kProfileCount>& Profiles() { return profiles_; }

    // Read-only accessors for UI
    [[nodiscard]] const GoalClock&     Clock()       const { return clock_; }
    [[nodiscard]] GoalList*            List()              { return &active_list_; }
    // All 4 DoA rotations, pre-built at Initialize() — for the Elite Area picker's manual "start at zone N" entries, since the real rotation is only known once you're actually in DoA.
    [[nodiscard]] const std::array<GoalList, 4>& DoAPresetCache() const { return doa_preset_cache_; }
    [[nodiscard]] bool RunComplete()                 const { return run_complete_; }
    [[nodiscard]] bool RunFailed()                   const { return run_failed_; }
    // Includes the in-progress pause so the display ticks up live while paused, not just on resume.
    [[nodiscard]] double TotalPausedReal()           const {
        return total_paused_real_ + (manually_paused_ ? manual_pause_accum_ : 0.0);
    }

    void NewActiveList(const char* name);
    // clear_preset: false for internal auto-saves (e.g. UpdateReferenceIfPB) that shouldn't silently turn an auto-detected preset into a plain user list mid-session — only an explicit user Save click should do that.
    void SaveActiveList(bool clear_preset = true);
    void LoadActiveList(const std::wstring& path);
    // Replaces the active list wholesale with an already-built one (e.g. from SCPresets); see the .cpp for why this isn't just NewActiveList() + mutating List()->goals after.
    void SetActiveList(GoalList list);
    // Cached — GetSavedLists() is a real directory_iterator disk scan and used to run unconditionally every ImGui frame the Splits settings section was open. See the cache fields below for invalidation.
    [[nodiscard]] std::vector<std::pair<std::string, std::wstring>> GetSavedLists() const;

    // Profile-specific subfolder paths for templates and run history.
    [[nodiscard]] std::wstring ActiveSplitsFolder() const;
    [[nodiscard]] std::wstring ActiveRunsFolder()   const;

    // Comparison data actually fed to the goal list's Ahead/Behind display — selects among the PB/average/last-run splits per ActiveProfile().comparison_mode.
    [[nodiscard]] const std::vector<double>& CompareSplits()     const;
    [[nodiscard]] const std::vector<double>& CompareSplitsGame() const;

    [[nodiscard]] const std::vector<RecentRun>& RecentRuns() const { return recent_runs_; }

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
    // All 4 rotations pre-built at Initialize(), not at the zone-transition tick — building fresh there could still be in progress when Room 1's own door-close event fires, missing its one-shot start_trigger.
    std::array<GoalList, 4> doa_preset_cache_;

    int  key_start_ = 0;
    int  key_reset_ = 0;
    int  key_split_ = 0;
    bool key_start_prev_ = false;
    bool key_reset_prev_ = false;
    bool key_split_prev_ = false;

    Color color_completed_ = Colors::RGB(0,   255, 0  );   // goal label: done
    Color color_active_    = Colors::RGB(255, 255, 255);   // goal label: current objective
    Color color_real_time_ = Colors::RGB(230, 230, 230);   // real time text; segment uses muted
    Color color_game_time_ = Colors::RGB(153, 217, 255);   // game time text; segment uses muted
    Color color_pb_ahead_  = Colors::RGB(255, 217,   0);   // comparison delta: ahead (of PB/Average/Sum of Best per comparison_mode)
    Color color_pb_behind_ = Colors::RGB(255, 102, 102);   // comparison delta: behind; also failed goal label

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
    GW::HookEntry on_agent_state_;
    GW::HookEntry on_quest_update_;
    GW::HookEntry on_quest_details_changed_;
    GW::HookEntry on_quest_remove_;

    void SaveResumeState();
    void DeleteResumeState();
    void SaveCompletedRun();
    void FailRun(const char* reason = "party defeated");
    void SaveRunToHistory(bool failed);
    // refresh_comparisons=false only rescans pb_splits_/pb_total_real_ (for UpdateReferenceIfPB's is-this-a-new-PB check), leaving avg_splits_/best_seg_splits_ untouched so a just-finished run's "Run complete!" screen still compares against the PRE-run history rather than itself.
    void LoadPB(bool refresh_comparisons = true);
    void UpdateReferenceIfPB();
    // Shared by LoadActiveList/SetActiveList/ResetRun — resets every run-progress flag to its pre-run state (not the clock/engine, which each caller handles differently around this call).
    void ResetRunFlags();
    // "A run just began" bookkeeping shared by StartRun(), Running's movement-detected start, and Manual/SC's first-goal-fired auto-start — each has a different trigger condition but does this same work once triggered.
    void BeginRun(const char* reason);
    // Detach -> populate -> Attach -> LoadPB, shared by LoadActiveList/SetActiveList/ApplyResume — each does something different afterward (Reset+ResetRunFlags vs. Restore+goal-status replay), but this core must run in this order regardless (Attach-time setup like starts_immediately needs active_list_ already populated).
    void ReplaceActiveList(const std::function<void()>& populate);
    // Filesystem-safe "<active_list_.name>.json" path under ActiveRunsFolder() — shared by LoadPB (reads it) and SaveRunToHistory (reads+writes it).
    [[nodiscard]] std::wstring RunHistoryFilePath() const;

    // Timer policy: auto-fail conditions and Manual's per-goal-type auto-start rules. Deliberately separate from GoalEngine's fire/complete switch — trigger firing and clock policy are independent pieces, so some structural duplication between the two switches is accepted.
    void ApplyTimerPolicy(bool just_entered_map);

    // SC only: swaps in the matching live-built preset when entering a dungeon/elite area with no run in progress. Runs before engine_.Update() each tick, never interrupts a run underway, and never overrides a non-preset (user-authored) list. DoA is handled first via pending_doa_file_id_/pending_doa_spawn_ since its zone rotation isn't map_id-driven.
    void ApplySCAutoLoadPreset(bool just_entered_map);

    bool        run_complete_   = false;
    bool        run_failed_     = false;
    std::string run_char_name_;
    int64_t     run_start_unix_ = 0;

    std::vector<double> pb_splits_;
    std::vector<double> pb_splits_game_;
    double              pb_total_real_ = std::numeric_limits<double>::quiet_NaN();
    // Mean of every non-failed run's time at each goal index (a run reaching only goal 3 of 5 still contributes to goals 0-2). All three (pb/avg/best-seg) recompute together in LoadPB().
    std::vector<double> avg_splits_;
    std::vector<double> avg_splits_game_;
    // Cumulative sum of each leg's fastest-ever segment across non-failed runs — the theoretical best if every best segment lined up in one run.
    std::vector<double> best_seg_splits_;
    std::vector<double> best_seg_splits_game_;

    std::vector<RecentRun> recent_runs_;

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

    // ---- Nuzlocke: Death Rules + Points state and behavior (see NuzlockeState.h / Windows/Splits/Nuzlocke.cpp) ----
    NuzlockeState nuzlocke_;

    static constexpr std::array<const char*, kProfileCount> kProfileSections = {
        "Splits.Manual", "Splits.Running", "Splits.SC"
    };
    // Shared by ActiveSplitsFolder()/ActiveRunsFolder() — one lookup instead of two matching 3-way if chains.
    static constexpr std::array<const wchar_t*, kProfileCount> kProfileFolderNames = {
        L"manual\\", L"running\\", L"sc\\"
    };

    struct ColorField { const char* key; Color SplitsWindow::* member; };
    // Shared by LoadSettings/SaveSettings so both iterate the same list instead of Save repeating it as 6 flat lines.
    static constexpr std::array<ColorField, 6> kColorFields = {{
        {"color_completed",  &SplitsWindow::color_completed_},
        {"color_active",     &SplitsWindow::color_active_},
        {"color_real_time",  &SplitsWindow::color_real_time_},
        {"color_game_time",  &SplitsWindow::color_game_time_},
        {"color_pb_ahead",   &SplitsWindow::color_pb_ahead_},
        {"color_pb_behind",  &SplitsWindow::color_pb_behind_},
    }};

    // Shared by LoadSettings/SaveSettings — one table instead of each field written out separately on both sides.
    struct NuzlockeBoolField { const char* key; bool NuzlockeState::* member; };
    static constexpr std::array<NuzlockeBoolField, 3> kNuzlockeBoolFields = {{
        {"nuzlocke_death_rules_enabled", &NuzlockeState::death_rules_enabled},
        {"nuzlocke_merge_hench_by_name", &NuzlockeState::merge_hench_by_name},
        {"nuzlocke_points_enabled",      &NuzlockeState::points_enabled},
    }};
    struct NuzlockeLivesField { const char* key; int NuzlockeState::* member; };
    static constexpr std::array<NuzlockeLivesField, 3> kNuzlockeLivesFields = {{
        {"nuzlocke_hero_lives",   &NuzlockeState::hero_lives},
        {"nuzlocke_hench_lives",  &NuzlockeState::hench_lives},
        {"nuzlocke_player_lives", &NuzlockeState::player_lives},
    }};
    struct NuzlockePointField { const char* key; int NuzlockePointValues::* member; };
    static constexpr std::array<NuzlockePointField, 8> kNuzlockePointFields = {{
        {"nuzlocke_points_manual",       &NuzlockePointValues::manual},
        {"nuzlocke_points_missions",     &NuzlockePointValues::missions},
        {"nuzlocke_points_explorables",  &NuzlockePointValues::explorables},
        {"nuzlocke_points_towns",        &NuzlockePointValues::towns},
        {"nuzlocke_points_titles",       &NuzlockePointValues::titles},
        {"nuzlocke_points_reach_level",  &NuzlockePointValues::reach_level},
        {"nuzlocke_points_quest",        &NuzlockePointValues::quest},
        {"nuzlocke_points_skill_learnt", &NuzlockePointValues::skill_learnt},
    }};
};
