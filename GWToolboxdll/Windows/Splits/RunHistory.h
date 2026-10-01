#pragma once

#include <Windows/Splits/SplitsProfile.h>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

struct GoalList;

struct RecentRunGoal {
    std::string label;
    double      real_time = 0.0;
    double      game_time = 0.0;
    bool        completed = false;
    double      start_real_time = -1.0; // -1 = not recorded
    double      start_game_time = -1.0;
};
struct RecentRun {
    double                     total_real     = 0.0;
    bool                       failed         = false;
    int64_t                    utc_start      = 0;
    std::string                start_zone;    // DoA rotation, "" otherwise
    std::vector<RecentRunGoal> goals;
    // 1-based file position (oldest = 1). Shift on delete / 200 cap.
    int                        attempt_number = 0;
};

// One comparison mode's per-goal times; grouped so real/game/start can't be mixed across modes.
struct ComparisonSplits {
    std::vector<double> real;
    std::vector<double> game;
    // SC own starts only. SumOfBest has none: empty.
    std::vector<double> start_real;
    std::vector<double> start_game;
};

class RunHistory {
public:
    // refresh_comparisons=false: PB only, so "Run complete!" compare against pre-run history.
    void Load(const std::filesystem::path& runs_path, int goal_count, bool refresh_comparisons = true);

    // Cap 200, oldest out first, protected runs stay. No reload.
    void Save(const std::filesystem::path& runs_path, const GoalList& active_list, bool failed,
              double total_real, int64_t run_start_unix, const std::string& run_char_name,
              double total_paused, const std::string& start_zone);

    [[nodiscard]] const ComparisonSplits& Comparison(SplitsProfile::ComparisonMode mode) const;

    [[nodiscard]] const std::vector<RecentRun>& RecentRuns() const { return recent_runs_; }
    // 0 = no PB.
    [[nodiscard]] int    PBAttemptNumber() const { return pb_attempt_number_; }
    [[nodiscard]] double PBTotalReal()     const { return pb_total_real_; } // NaN = no PB
    // Attempt that set each leg best (0 = never). Real/game separate.
    [[nodiscard]] const std::vector<int>& BestSegRealAttempt() const { return best_seg_real_attempt_; }
    [[nodiscard]] const std::vector<int>& BestSegGameAttempt() const { return best_seg_game_attempt_; }

    // Not cached: delete must show at once. Newest first.
    [[nodiscard]] std::vector<RecentRun> LoadFull(const std::filesystem::path& runs_path) const;
    void DeleteRuns(const std::filesystem::path& runs_path, const std::vector<int>& attempt_numbers, int goal_count);
    void Clear(const std::filesystem::path& runs_path, int goal_count);

private:
    // Old runs store -1.0 start: treat as missing.
    ComparisonSplits pb_;
    double           pb_total_real_ = std::numeric_limits<double>::quiet_NaN();
    int              pb_attempt_number_ = 0;

    // Partial non-failed runs still count for legs they reached.
    ComparisonSplits avg_;

    ComparisonSplits sob_;
    std::vector<int> best_seg_real_attempt_;
    std::vector<int> best_seg_game_attempt_;

    std::vector<RecentRun> recent_runs_;
};
