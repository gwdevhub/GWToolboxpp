#pragma once

#include <ToolboxModule.h>

class ToolboxUIElement : public ToolboxModule {
    friend class ToolboxSettings;

public:
    [[nodiscard]] bool IsUIElement() const override { return true; }

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9*) override { }

    void UpdateLocationAgainstSnappedFrame();
    static void UpdateCachedFrameStates();

    [[nodiscard]] virtual const char* UIName() const;
    //virtual const char* SettingsName() const override { return UIName(); }

    void Initialize() override;
    void Terminate() override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;

    void SaveSettings(SettingsDoc& doc) override;

    // Called when the game switches between mobile and normal mode
    void OnMobileModeChanged(bool is_mobile);

    [[nodiscard]] virtual ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags = 0) const;

    // returns true if clicked
    virtual bool DrawTabButton(bool show_icon = true, bool show_text = true, bool center_align_text = true);

    // Draw a small floating button that toggles this element's visibility
    virtual void DrawBreakoutButton(IDirect3DDevice9*);

    virtual bool ToggleVisible() { return visible = !visible; }

    [[nodiscard]] virtual bool ShowOnWorldMap() const { return false; }

    [[nodiscard]] const char* TypeName() const override { return "ui element"; }

    void RegisterSettingsContent() override;

    virtual void DrawSizeAndPositionSettings();

    bool visible = false;
    bool lock_move = false;
    bool lock_size = false;
    bool show_menubutton = false;
    bool auto_size = false;
    bool show_breakout_button = false;
    bool lock_breakout_button = false;
    std::array<float, 2> breakout_pos = {60.f, 60.f};
    bool pending_breakout_pos = false;
    // Runtime-only: set once the button has been positioned (from saved settings or a computed default)
    bool breakout_pos_set = false;

    // Mobile-mode layout settings (separate from normal-mode settings above)
    bool mobile_lock_move = false;
    bool mobile_lock_size = false;
    bool mobile_auto_size = false;

    // Returns lock_move/lock_size/auto_size for the current game mode
    [[nodiscard]] bool IsMoveLocked() const;
    [[nodiscard]] bool IsSizeLocked() const;
    [[nodiscard]] bool IsAutoSized() const;

    bool* GetVisiblePtr();

    bool has_titlebar = false;
    bool show_titlebar = true;
    bool show_closebutton = true;

    // Auto-resize width/height when window collapses or expands
    bool auto_resize_on_collapse = false;
    std::array<float, 2> collapsed_size = {0.f, 0.f};
    std::array<float, 2> expanded_size = {0.f, 0.f};

    // Settings panel tab: 0 = Normal, 1 = Mobile, -1 = auto (tracks current mode)
    int settings_active_tab = -1;

protected:
    bool can_show_in_main_window = true;
    bool has_closebutton = false;
    bool is_resizable = true;
    bool is_movable = true;

    std::string snapped_frame_label;
    std::string mobile_snapped_frame_label;

    // Relative offset to the snapped GW frame's top-left corner (screen coords = frame_pos + snap_offset)
    std::array<float, 2> snap_offset = {0.f, 0.f};
    std::array<float, 2> mobile_snap_offset = {0.f, 0.f};
    // Set true when snap label changes; cleared once frame position is known and snap_offset is initialized
    bool snap_offset_needs_init = false;
    bool mobile_snap_offset_needs_init = false;

    // Stored positions for each mode (used when switching modes)
    bool has_normal_layout = false;
    std::array<float, 2> normal_pos = {};
    std::array<float, 2> normal_size = {};
    bool has_mobile_layout = false;
    std::array<float, 2> mobile_pos = {};
    std::array<float, 2> mobile_size = {};

    float min_size[2] = { 250.f,90.f };
    float max_size[2] = { FLT_MAX, FLT_MAX };

    virtual void ShowVisibleRadio();

    ImVec2 last_frame_pos;

    // Collapse-state tracking for auto_resize_on_collapse (mutable: used in const GetWinFlags)
    mutable bool prev_was_collapsed = false;
    mutable bool collapse_size_initialized = false;
};
