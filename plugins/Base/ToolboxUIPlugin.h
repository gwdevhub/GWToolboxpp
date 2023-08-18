#pragma once

#include "ToolboxPlugin.h"

class ToolboxUIPlugin : public ToolboxPlugin {
public:
    bool* GetVisiblePtr() override;

    void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    bool CanTerminate() override;
    void SignalTerminate() override;
    void Terminate() override;
    bool HasSettings() const override { return true; }
    void DrawSettings() override;
    void LoadSettings(const wchar_t*) override;
    void SaveSettings(const wchar_t*) override;

protected:
    ImGuiWindowFlags GetWinFlags(ImGuiWindowFlags flags = 0) const;

    bool show_closebutton = true;
    bool can_show_in_main_window = false;
    bool has_closebutton = false;
    bool is_resizable = true;
    bool is_movable = true;

    bool lock_move = false;
    bool lock_size = false;
    bool show_menubutton = false;

private:
    bool plugin_visible = true;
};
