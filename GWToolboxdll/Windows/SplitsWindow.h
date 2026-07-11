#pragma once

#include <array>
#include <string>
#include <vector>
#include <ToolboxWindow.h>

#include <Windows/Splits/GoalClock.h>
#include <Windows/Splits/GoalEngine.h>
#include <Windows/Splits/GoalList.h>
#include <Windows/Splits/SplitsProfile.h>
#include <Windows/Splits/SplitsGoalListWindow.h>

inline constexpr int kProfileCount = 2;

// ---------------------------------------------------------------------------
// SplitsWindow — speedrun split timer, ported from the GWSplits plugin.
// ---------------------------------------------------------------------------
class SplitsWindow : public ToolboxWindow {
    SplitsWindow() = default;
    ~SplitsWindow() override = default;

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
    [[nodiscard]] std::vector<std::pair<std::string, std::wstring>> GetSavedLists() const;

    // Profile-specific subfolder paths for templates and run history.
    [[nodiscard]] std::wstring ActiveSplitsFolder() const;
    [[nodiscard]] std::wstring ActiveRunsFolder()   const;

    [[nodiscard]] const std::vector<double>& PBSplits()     const { return pb_splits_; }
    [[nodiscard]] const std::vector<double>& PBSplitsGame() const { return pb_splits_game_; }
    [[nodiscard]] double PBTotal()                          const { return pb_total_real_; }

    [[nodiscard]] bool        HasPendingResume()  const { return pending_resume_; }
    [[nodiscard]] const char* PendingResumeName() const { return pending_resume_name_.c_str(); }
    void ApplyResume();
    void DiscardResume();

private:
    GoalClock             clock_;
    GoalEngine            engine_;
    GoalList              active_list_;
    SplitsGoalListWindow  window_;

    // Profiles: 0=Manual, 1=Running
    std::array<SplitsProfile, kProfileCount> profiles_ = {
        MakeManualProfile(), MakeRunningProfile()
    };
    int active_profile_idx_ = 0;

    GW::Constants::MapID  last_map_                 = GW::Constants::MapID::None;
    bool                  last_was_explorable_      = false;
    // Set by the InstanceLoadInfo bus event; consumed once per Update() tick.
    bool                  pending_map_enter_        = false;
    bool                  pending_came_from_explorable_ = false;

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
    void FailRun();
    void SaveRunToHistory(bool failed);
    void LoadPB();
    void UpdateReferenceIfPB();

    bool        run_complete_   = false;
    bool        run_failed_     = false;
    std::string run_char_name_;
    int         run_char_level_ = 0;
    int64_t     run_start_unix_ = 0;

    std::vector<double> pb_splits_;
    std::vector<double> pb_splits_game_;
    double              pb_total_real_ = std::numeric_limits<double>::quiet_NaN();

    float       resume_save_timer_   = 0.f;
    bool        pending_resume_      = false;
    std::string pending_resume_name_;
    std::string pending_resume_data_;

    static constexpr std::array<const char*, kProfileCount> kProfileSections = {
        "Splits.Manual", "Splits.Running"
    };
};
