#pragma once

#include <map>
#include <set>

// Forward declarations
class SplitsWindow;
class ToolboxIni;
class SettingsDoc;
struct GoalList;
struct GoalEntry;
class GoalClock;

// ---------------------------------------------------------------------------
// SplitsGoalListWindow — runtime display + settings UI for the Splits window.
// Per-profile display settings (colors, columns, etc.) now live in
// SplitsProfile and are accessed via plugin.ActiveProfile().
// ---------------------------------------------------------------------------
class SplitsGoalListWindow {
public:
    void Draw(SplitsWindow& window);
    void DrawSettings(SplitsWindow& window);
    // LoadSettings/SaveSettings for non-profile state (edit buffers, etc.)
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy);
    void SaveSettings(SettingsDoc& doc);

private:
    bool DrawHeaderRow(const GoalList& list, int header_idx, const struct SplitsProfile& profile);
    void DrawGoalRow(const GoalEntry& g, const GoalClock& clock, int index,
                     bool is_current,
                     double pb_split_real, double pb_seg_real,
                     double pb_split_game, double pb_seg_game,
                     const struct SplitsProfile& profile);
    void DrawMissionBatchPicker(SplitsWindow& window);
    void DrawExplorableBatchPicker(SplitsWindow& window);
    void DrawTownBatchPicker(SplitsWindow& window);
    void DrawDungeonBatchPicker(SplitsWindow& window);
    void DrawEliteAreaBatchPicker(SplitsWindow& window);
    void DrawTitlePicker(SplitsWindow& window);
    void DrawRunningLegPicker(SplitsWindow& window);

    char edit_label_[128]     = {};
    int  edit_trigger_type_   = 0; // -1 = Header (no trigger), -2 = Quest (Pickup+Complete pair)
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

    std::set<int> batch_dungeon_checked_;
    char          dungeon_filter_buf_[128] = {};

    // Elite area picker — each keyed by checkpoint array index (not param1, since Urgoz/Deep
    // mix DoorOpen/DisplayDialogue/ServerMessage triggers whose param1/pattern values could
    // otherwise collide).
    std::set<int> batch_fow_checked_;
    std::set<int> batch_uw_checked_;
    std::set<int> batch_urgoz_checked_;
    std::set<int> batch_deep_checked_;

    // Running profile leg picker
    int  run_map_id_    = 0;
    char run_filter_[128] = {};

    int  edit_title_id_       = 0xff;
    int  edit_title_rank_     = -1; // 0-based tier index; -1 = unset (defaults to max on first use)
    char title_filter_buf_[128] = {};

    // Manual entry for both — matches wiki numbering, no master name list needed for either.
    int  edit_quest_id_ = 0;
    int  edit_skill_id_ = 0;

    // Model ID is visible live in-game via the Info window's target panel, no wiki needed.
    int  edit_mob_id_         = 0;
    int  edit_mob_kill_count_ = 1;
};
