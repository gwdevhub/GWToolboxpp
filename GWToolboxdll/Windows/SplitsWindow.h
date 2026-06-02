#pragma once

#include <ToolboxWindow.h>

#include <GWCA/Utilities/Hook.h>

#include <Windows/Splits/GoalClock.h>
#include <Windows/Splits/GoalEngine.h>
#include <Windows/Splits/GoalList.h>
#include <Windows/Splits/SplitsGoalListWindow.h>

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

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;

    // Called by UI
    void StartRun();
    void ResetRun();
    void TriggerManualSplit();

    // Keybind accessors (VK codes; 0 = unbound)
    int& KeyStart() { return key_start_; }
    int& KeyReset() { return key_reset_; }
    int& KeySplit() { return key_split_; }

    // Read-only accessors for UI
    [[nodiscard]] const GoalClock&   Clock()       const { return clock_; }
    [[nodiscard]] const GoalEngine&  Engine()      const { return engine_; }
    [[nodiscard]] GoalList*          List()              { return &active_list_; }
    [[nodiscard]] GW::Constants::MapID CurrentMap() const { return last_map_; }
    [[nodiscard]] bool RunComplete()               const { return run_complete_; }

    void NewActiveList(const char* name);
    void SaveActiveList();
    void LoadActiveList(const std::wstring& path);
    [[nodiscard]] std::vector<std::pair<std::string, std::wstring>> GetSavedLists() const;

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

    GW::Constants::MapID  last_map_            = GW::Constants::MapID::None;
    uint32_t              last_instance_time_  = 0;
    bool                  vq_complete_         = false;
    bool                  last_was_explorable_ = false;

    int  key_start_ = 0;
    int  key_reset_ = 0;
    int  key_split_ = 0;
    bool key_start_prev_ = false;
    bool key_reset_prev_ = false;
    bool key_split_prev_ = false;

    GW::HookEntry on_mission_complete_hook_;
    GW::HookEntry on_objective_complete_hook_;

    std::wstring splits_folder_; // e.g. <data>/splits/
    std::wstring runs_folder_;   // e.g. <data>/splits/runs/

    void SaveResumeState();
    void DeleteResumeState();
    void SaveCompletedRun();
    void LoadPB();

    bool        run_complete_        = false;
    std::string run_char_name_;
    int         run_char_level_      = 0;

    std::vector<double> pb_splits_;
    std::vector<double> pb_splits_game_;
    double              pb_total_real_     = std::numeric_limits<double>::quiet_NaN();

    float       resume_save_timer_   = 0.f;
    bool        pending_resume_      = false;
    std::string pending_resume_name_;
    std::string pending_resume_data_;
};
