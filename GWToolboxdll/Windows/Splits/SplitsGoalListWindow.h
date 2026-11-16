#pragma once

#include <Windows/Splits/SplitsProfile.h>

class SplitsWindow;
struct GoalList;
struct GoalEntry;
class GoalClock;
struct ComparisonSplits;
struct RecentRun;

class SplitsGoalListWindow {
public:
    void Draw(SplitsWindow& splits);
    void DrawSettings(SplitsWindow& splits);

private:
    // History rows: no live game reads (title rank, kill counter), and gold deltas where the attempt holds the best segment.
    struct GoalRowOptions {
        bool live = true;
        bool gold_real = false;
        bool gold_game = false;
    };

    bool DrawHeaderRow(const GoalList& list, int header_idx, const SplitsProfile& profile);
    void DrawGoalRow(
        const GoalEntry& g, const GoalClock& clock, bool is_current, double pb_split_real, double pb_seg_real, double pb_split_game, double pb_seg_game, double pb_start_real, double pb_start_game, const SplitsProfile& profile, SplitsWindow& splits,
        const GoalRowOptions& opts
    );
    // Shared by the live window and Run History, so an attempt looks like the main window frozen at its end.
    // attempt_number 0 = the live run; otherwise gold comes from the best-segment attempts.
    void DrawGoalRows(const GoalList& list, const GoalClock& clock, const ComparisonSplits& cmp, const SplitsProfile& profile, SplitsWindow& splits, int attempt_number);
    void DrawRecentRunsSection(SplitsWindow& splits);
    // Expanded attempt, in Recent Runs or Run History: built once per history load.
    void DrawAttempt(SplitsWindow& splits, const RecentRun& run, SplitsProfile::ComparisonMode mode);
    void DrawRunHistoryViewer(SplitsWindow& splits);

    void DrawProfileSwitcher(SplitsWindow& splits);
    void DrawTimeAndBehaviorColumn(SplitsWindow& splits);
    void DrawKeybindsAndColorsColumn(SplitsWindow& splits);
    void DrawGoalListManagementColumn(SplitsWindow& splits);
    void DrawPlainLoadCombo(SplitsWindow& splits);
    void DrawSCLoadCombo(SplitsWindow& splits);
    void DrawSCCreateVariantModal(SplitsWindow& splits);
    void DrawSCGoalsSummary(SplitsWindow& splits);
    void DrawEditableGoalsList(SplitsWindow& splits);
    void DrawStandardAddGoalForm(SplitsWindow& splits);

    void DrawMissionBatchPicker(SplitsWindow& splits);
    void DrawExplorableBatchPicker(SplitsWindow& splits);
    void DrawTownBatchPicker(SplitsWindow& splits);
    void DrawDungeonBatchPicker(SplitsWindow& splits);
    void DrawEliteAreaBatchPicker(SplitsWindow& splits);
    void DrawTitlePicker(SplitsWindow& splits);
};
