#pragma once

#include "ToolboxPlugin.h"

class ToolboxUIPlugin : public ToolboxPlugin {
public:
    bool* GetVisiblePtr() override { return &plugin_visible; }
    virtual bool ToggleVisible() { return plugin_visible = !plugin_visible; }

    void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    bool CanTerminate() override;
    void Terminate() override;
    bool HasSettings() const override { return true; }
    void LoadSettings(const wchar_t*) override;
    void SaveSettings(const wchar_t*) override;

protected:
    void DrawSizeAndPositionSettings();

    bool plugin_visible = true;
    bool show_closebutton = true;
    bool can_show_in_main_window = true;
    bool has_closebutton = false;
    bool is_resizable = true;
    bool is_movable = true;

    bool lock_move = false;
    bool lock_size = false;
    bool show_menubutton = false;
};
