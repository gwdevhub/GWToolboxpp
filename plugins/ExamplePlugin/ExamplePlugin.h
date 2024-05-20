#pragma once

#include <ToolboxUIPlugin.h>

class ExamplePlugin : public ToolboxUIPlugin {
public:
    ExamplePlugin()
    {
        can_show_in_main_window = false;
        show_title = false;
        can_collapse = false;
        can_close = false;
    }
    ~ExamplePlugin() override = default;

    const char* Name() const override { return "Example Plugin"; }

    void LoadSettings(const wchar_t*) override;
    void SaveSettings(const wchar_t*) override;
    void DrawSettings() override;
    void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;
};
