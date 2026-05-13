#pragma once

#include <map>
#include <set>

// Forward declarations
class SplitsWindow;
struct GoalList;
struct GoalEntry;
class GoalClock;

// ---------------------------------------------------------------------------
// SplitsGoalListWindow — runtime display + settings UI for the Splits window.
// ---------------------------------------------------------------------------
class SplitsGoalListWindow {
public:
    void Draw(SplitsWindow& window);
    void DrawSettings(SplitsWindow& window);

private:
    void DrawMissionRow(const GoalEntry& mis, const GoalEntry* bon, const GoalClock& clock,
                        bool is_current, bool has_prior_split,
                        double pb_split_mis, double pb_split_bon);
    void DrawGoalRow(const GoalEntry& g, const GoalClock& clock, int index,
                     bool is_current, bool has_prior_split, double pb_split);
    void DrawMissionBatchPicker(SplitsWindow& window);
    void DrawExplorableBatchPicker(SplitsWindow& window);
    void DrawTownBatchPicker(SplitsWindow& window);
    void DrawTitlePicker(SplitsWindow& window);

    bool show_game_time_ = true;
    bool show_real_time_ = true;
    bool show_segment_   = true;
    bool show_town_time_ = true;
    int  pb_basis_       = 0; // 0 = real, 1 = game

    char edit_label_[128]     = {};
    int  edit_trigger_type_   = 0;
    int  edit_map_id_         = 0;
    int  edit_level_          = 1;
    char list_name_buf_[64]   = {};
    char map_filter_buf_[128] = {};

    std::set<int>          batch_mis_checked_;
    std::set<int>          batch_bon_checked_;
    bool                   batch_hm_ = false;

    std::map<int, uint8_t> batch_exp_checked_;
    char                   exp_filter_buf_[128] = {};

    std::map<int, uint8_t> batch_town_checked_;
    char                   town_filter_buf_[128] = {};

    int  edit_title_id_       = 0xff;
    char title_filter_buf_[128] = {};
};
