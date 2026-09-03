#pragma once

#include <IconsFontAwesome5.h>
#include <ToolboxUIPlugin.h>

class HeartbeatPlugin : public ToolboxUIPlugin {
public:
    HeartbeatPlugin() { can_show_in_main_window = true; }
    ~HeartbeatPlugin() override = default;

    [[nodiscard]] const char* Name() const override { return "Heartbeat Timer"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_HEARTBEAT; }

    void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocs, HMODULE toolbox_dll) override;
    void Draw(IDirect3DDevice9* pDevice) override;
};
