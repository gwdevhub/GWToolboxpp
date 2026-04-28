#pragma once

#include <ToolboxUIPlugin.h>

#include <IconsFontAwesome5.h>

class PitsSoulsWindow : public ToolboxUIPlugin {
public:
    PitsSoulsWindow()
    {
    }
    ~PitsSoulsWindow() override = default;

    const char* Name() const override { return "PitsSouls"; }
    const char* Icon() const override { return ICON_FA_GHOST; }

    void Update(float) override;
    void DrawSettings() override;
    void Draw(IDirect3DDevice9* pDevice) override;

    void Initialize(ImGuiContext* ctx, ImGuiAllocFns fns, HMODULE toolbox_dll);
    void SignalTerminate();
};
