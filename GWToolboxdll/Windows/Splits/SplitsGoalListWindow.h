#pragma once

class SplitsWindow;
struct SplitsProfile;
struct GoalList;
struct GoalEntry;
class GoalClock;

class SplitsGoalListWindow {
public:
    void Draw(SplitsWindow& splits);
    void DrawSettings(SplitsWindow& splits);

private:
    bool DrawHeaderRow(const GoalList& list, int header_idx, const SplitsProfile& profile);
    void DrawGoalRow(const GoalEntry& g, const GoalClock& clock, bool is_current,
                     double pb_split_real, double pb_seg_real,
                     double pb_split_game, double pb_seg_game,
                     double pb_start_real, double pb_start_game,
                     const SplitsProfile& profile, SplitsWindow& splits);
    void DrawRecentRunsSection(SplitsWindow& splits);
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
