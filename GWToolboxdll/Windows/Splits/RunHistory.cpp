#include "stdafx.h"

#include "RunHistory.h"

#include <Modules/Resources.h>
#include <Windows/Splits/GoalEntry.h>
#include <Windows/Splits/GoalList.h>
#include <glaze/glaze.hpp>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <optional>
#include <set>

// glaze reflection need external linkage.
namespace RunHistoryJson {
    struct SerializedRunSplit {
        double real_time = 0.0;
        double game_time = 0.0;
        // -1.0 = not fired, or old run before field existed. Load() treat same.
        double start_real_time = -1.0;
        double start_game_time = -1.0;
    };

    struct SerializedRunGoal {
        std::string label;
        std::string status; // "Completed" / "Failed" / "Started" / "NotStarted"
        double real_time = 0.0;
        double game_time = 0.0;
    };

    struct SerializedRun {
        double total_real = 0.0;
        std::vector<SerializedRunSplit> splits;
        std::optional<bool> failed;
        std::optional<int64_t> utc_start;
        std::optional<std::string> character_name;
        std::optional<std::vector<SerializedRunGoal>> goals;
        std::optional<double> total_paused;
        // Starting zone for rotating areas (DoA), so zone times can later be split by run order.
        std::optional<std::string> start_zone;
    };
}
using namespace RunHistoryJson;

namespace {
    RecentRun ToRecentRun(const SerializedRun& run, const int attempt_number)
    {
        RecentRun rr;
        rr.attempt_number = attempt_number;
        rr.total_real      = run.total_real;
        rr.failed          = run.failed.value_or(false);
        rr.utc_start        = run.utc_start.value_or(0);
        rr.start_zone       = run.start_zone.value_or("");
        if (run.goals) {
            rr.goals.reserve(run.goals->size());
            for (size_t i = 0; i < run.goals->size(); ++i) {
                const auto& g = (*run.goals)[i];
                RecentRunGoal rg{g.label, g.real_time, g.game_time, g.status == "Completed"};
                // splits[] is saved in the same loop as goals[], so indices line up.
                if (i < run.splits.size()) {
                    rg.start_real_time = run.splits[i].start_real_time;
                    rg.start_game_time = run.splits[i].start_game_time;
                }
                rr.goals.push_back(std::move(rg));
            }
        }
        return rr;
    }

    bool ReadRuns(const std::filesystem::path& runs_path, std::vector<SerializedRun>& out)
    {
        std::string content;
        if (!Resources::ReadFile(runs_path, content)) return false;
        constexpr glz::opts opts{.error_on_unknown_keys = false};
        if (!glz::read<opts>(out, content)) return true;
        Log::ErrorW(L"Splits: failed to parse run history %s", runs_path.wstring().c_str());
        return false;
    }

    void WriteRuns(const std::filesystem::path& runs_path, const std::vector<SerializedRun>& runs)
    {
        if (!Resources::WriteFile(runs_path, glz::write<glz::opts{.prettify = true}>(runs).value_or(std::string{})))
            Log::ErrorW(L"Splits: failed to write run history %s", runs_path.wstring().c_str());
    }

    // Shared by Load() and ProtectedRunIndices() so both agree which run hold what.
    struct BestScan {
        std::optional<size_t> pb_idx;
        double pb_total = std::numeric_limits<double>::infinity();
        std::vector<double> seg_real, seg_game;                 // inf = no run reached leg
        std::vector<std::optional<size_t>> seg_real_idx, seg_game_idx;
    };

    BestScan ScanBests(const std::vector<SerializedRun>& runs, const int goal_count)
    {
        const auto n = static_cast<size_t>(goal_count);
        BestScan s;
        s.seg_real.assign(n, std::numeric_limits<double>::infinity());
        s.seg_game.assign(n, std::numeric_limits<double>::infinity());
        s.seg_real_idx.resize(n);
        s.seg_game_idx.resize(n);
        for (size_t ri = 0; ri < runs.size(); ++ri) {
            const auto& run = runs[ri];
            if (run.failed.value_or(false) || run.splits.size() < n) continue;
            if (run.total_real < s.pb_total) { s.pb_total = run.total_real; s.pb_idx = ri; }
            double prev_real = 0.0, prev_game = 0.0;
            for (size_t g = 0; g < n; ++g) {
                const auto& split = run.splits[g];
                // Own start when recorded (parallel/out-of-order goals); relay goals' start equals the previous end anyway.
                const double start_real = split.start_real_time >= 0.0 ? split.start_real_time : prev_real;
                const double start_game = split.start_game_time >= 0.0 ? split.start_game_time : prev_game;
                if (split.real_time - start_real < s.seg_real[g]) { s.seg_real[g] = split.real_time - start_real; s.seg_real_idx[g] = ri; }
                if (split.game_time - start_game < s.seg_game[g]) { s.seg_game[g] = split.game_time - start_game; s.seg_game_idx[g] = ri; }
                prev_real = split.real_time;
                prev_game = split.game_time;
            }
        }
        return s;
    }

    // Never evict these: would regress PB/SumOfBest, not just drop old data.
    std::set<size_t> ProtectedRunIndices(const std::vector<SerializedRun>& runs, const int goal_count)
    {
        const BestScan s = ScanBests(runs, goal_count);
        std::set<size_t> protected_idx;
        if (s.pb_idx) protected_idx.insert(*s.pb_idx);
        for (const auto& i : s.seg_real_idx) if (i) protected_idx.insert(*i);
        for (const auto& i : s.seg_game_idx) if (i) protected_idx.insert(*i);
        return protected_idx;
    }
}

void RunHistory::Load(const std::filesystem::path& runs_path, const int goal_count, const bool refresh_comparisons)
{
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    pb_ = {};
    pb_total_real_     = nan;
    pb_attempt_number_ = 0;
    recent_runs_.clear();
    if (refresh_comparisons) {
        avg_ = {};
        sob_ = {};
        best_seg_real_attempt_.clear(); best_seg_game_attempt_.clear();
    }

    if (goal_count <= 0) return;
    const auto n = static_cast<size_t>(goal_count);

    std::vector<SerializedRun> runs;
    if (!ReadRuns(runs_path, runs)) return;

    const BestScan scan = ScanBests(runs, goal_count);

    if (scan.pb_idx) {
        const SerializedRun* best_run = &runs[*scan.pb_idx];
        pb_total_real_ = scan.pb_total;
        pb_attempt_number_ = static_cast<int>(*scan.pb_idx) + 1;
        pb_.real.resize(n, nan);
        pb_.game.resize(n, nan);
        pb_.start_real.resize(n, nan);
        pb_.start_game.resize(n, nan);
        for (size_t i = 0; i < std::min(n, best_run->splits.size()); ++i) {
            const auto& split = best_run->splits[i];
            pb_.real[i] = split.real_time;
            pb_.game[i] = split.game_time;
            // -1.0 = old run or never started: NaN, not bogus -1s.
            if (split.start_real_time >= 0.0) {
                pb_.start_real[i] = split.start_real_time;
                pb_.start_game[i] = split.start_game_time;
            }
        }
    }

    // Always, unlike Avg/SoB: just-finished run show at once.
    constexpr size_t kMaxRecentRuns = 5;
    const size_t take = std::min(kMaxRecentRuns, runs.size());
    recent_runs_.reserve(take);
    for (size_t i = 0; i < take; ++i) {
        const size_t idx = runs.size() - 1 - i;
        recent_runs_.push_back(ToRecentRun(runs[idx], static_cast<int>(idx) + 1));
    }

    if (!refresh_comparisons) return;

    {
        std::vector<double> sum_real(n, 0.0);
        std::vector<double> sum_game(n, 0.0);
        std::vector<int>    count(n, 0);
        // Own count: missing old Start must not pull average toward 0.
        std::vector<double> sum_start_real(n, 0.0);
        std::vector<double> sum_start_game(n, 0.0);
        std::vector<int>    count_start(n, 0);
        for (const auto& run : runs) {
            if (run.failed.value_or(false)) continue;
            for (size_t i = 0; i < std::min(n, run.splits.size()); ++i) {
                const auto& split = run.splits[i];
                sum_real[i] += split.real_time;
                sum_game[i] += split.game_time;
                ++count[i];
                if (split.start_real_time >= 0.0) {
                    sum_start_real[i] += split.start_real_time;
                    sum_start_game[i] += split.start_game_time;
                    ++count_start[i];
                }
            }
        }
        avg_.real.resize(n, nan);
        avg_.game.resize(n, nan);
        avg_.start_real.resize(n, nan);
        avg_.start_game.resize(n, nan);
        for (size_t i = 0; i < n; ++i) {
            if (count_start[i] > 0) {
                avg_.start_real[i] = sum_start_real[i] / count_start[i];
                avg_.start_game[i] = sum_start_game[i] / count_start[i];
            }
            if (count[i] > 0) {
                avg_.real[i] = sum_real[i] / count[i];
                avg_.game[i] = sum_game[i] / count[i];
            }
        }
    }

    {
        sob_.real.assign(n, nan);
        sob_.game.assign(n, nan);
        best_seg_real_attempt_.assign(n, 0);
        best_seg_game_attempt_.assign(n, 0);
        double cum_real = 0.0, cum_game = 0.0;
        for (size_t i = 0; i < n; ++i) {
            if (scan.seg_real_idx[i]) best_seg_real_attempt_[i] = static_cast<int>(*scan.seg_real_idx[i]) + 1;
            if (scan.seg_game_idx[i]) best_seg_game_attempt_[i] = static_cast<int>(*scan.seg_game_idx[i]) + 1;
        }
        for (size_t i = 0; i < n; ++i) {
            if (std::isinf(scan.seg_real[i])) break;
            cum_real += scan.seg_real[i];
            cum_game += scan.seg_game[i];
            sob_.real[i] = cum_real;
            sob_.game[i] = cum_game;
        }
    }
}

void RunHistory::Save(const std::filesystem::path& runs_path, const GoalList& active_list, const bool failed,
                       const double total_real, const int64_t run_start_unix,
                       const std::string& run_char_name, const double total_paused,
                       const std::string& start_zone)
{
    std::vector<SerializedRun> runs;
    if (!ReadRuns(runs_path, runs) && std::filesystem::exists(runs_path)) {
        // Unreadable, not missing (e.g. crash mid-write): set it aside instead of overwriting all history with one run.
        auto backup = runs_path;
        backup += std::format(L".{}.bak", std::time(nullptr));
        std::error_code ec;
        std::filesystem::rename(runs_path, backup, ec);
        Log::ErrorW(L"Splits: unreadable run history moved to %s", backup.wstring().c_str());
        runs.clear();
    }

    auto status_name = [](GoalStatus s) -> std::string {
        switch (s) {
            case GoalStatus::Started:   return "Started";
            case GoalStatus::Completed: return "Completed";
            case GoalStatus::Failed:    return "Failed";
            default:                    return "NotStarted";
        }
    };

    SerializedRun run;
    run.total_real      = total_real;
    run.failed          = failed;
    run.utc_start       = run_start_unix;
    run.character_name  = run_char_name;
    run.total_paused    = total_paused;
    if (!start_zone.empty()) run.start_zone = start_zone;

    std::vector<SerializedRunGoal> rgoals;
    for (const auto& g : active_list.goals) {
        if (g.is_header) continue;
        run.splits.push_back(SerializedRunSplit{g.split.real_time, g.split.game_time,
                                                 g.start_real_time, g.start_game_time});
        rgoals.push_back(SerializedRunGoal{g.label, status_name(g.status),
                                            g.split.real_time, g.split.game_time});
    }
    run.goals = std::move(rgoals);

    const int goal_count = static_cast<int>(run.splits.size()); // read before move
    runs.push_back(std::move(run));
    constexpr size_t kMaxRuns = 200;
    if (runs.size() > kMaxRuns) {
        // Oldest first, skip protected. Can end few over cap, never unbounded.
        const auto protected_idx = ProtectedRunIndices(runs, goal_count);
        std::vector<SerializedRun> kept;
        kept.reserve(runs.size());
        size_t to_remove = runs.size() - kMaxRuns;
        for (size_t i = 0; i < runs.size(); ++i) {
            if (to_remove > 0 && !protected_idx.contains(i)) {
                --to_remove;
                continue;
            }
            kept.push_back(std::move(runs[i]));
        }
        runs = std::move(kept);
    }

    WriteRuns(runs_path, runs);
}

const ComparisonSplits& RunHistory::Comparison(const SplitsProfile::ComparisonMode mode) const
{
    using CM = SplitsProfile::ComparisonMode;
    switch (mode) {
        case CM::Average:   return avg_;
        case CM::SumOfBest: return sob_;
        default:            return pb_;
    }
}

std::vector<RecentRun> RunHistory::LoadFull(const std::filesystem::path& runs_path) const
{
    std::vector<RecentRun> out;
    std::vector<SerializedRun> runs;
    if (!ReadRuns(runs_path, runs)) return out;

    out.reserve(runs.size());
    for (size_t i = runs.size(); i-- > 0; )
        out.push_back(ToRecentRun(runs[i], static_cast<int>(i) + 1));
    return out;
}

void RunHistory::DeleteRuns(const std::filesystem::path& runs_path, const std::vector<int>& attempt_numbers, const int goal_count)
{
    if (attempt_numbers.empty()) return;

    std::vector<SerializedRun> runs;
    if (!ReadRuns(runs_path, runs)) return;

    // Descending so erase not shift others.
    std::vector<int> sorted = attempt_numbers;
    std::sort(sorted.begin(), sorted.end(), std::greater<>());
    for (const int attempt : sorted) {
        const size_t idx = static_cast<size_t>(attempt - 1);
        if (attempt >= 1 && idx < runs.size())
            runs.erase(runs.begin() + static_cast<long>(idx));
    }

    WriteRuns(runs_path, runs);
    Load(runs_path, goal_count, true);
}

void RunHistory::Clear(const std::filesystem::path& runs_path, const int goal_count)
{
    // Write real [] not empty file: keep corrupt file distinct from cleared.
    Resources::WriteFile(runs_path, "[]");
    Load(runs_path, goal_count, true);
}
