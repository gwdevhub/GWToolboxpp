#pragma once

#include <ToolboxUIPlugin.h>

#include <IconsFontAwesome5.h>

class DhuumCalculator : public ToolboxUIPlugin {
public:
    DhuumCalculator() { can_show_in_main_window = true; }
    ~DhuumCalculator() override = default;

    const char* Name() const override { return "DhuumCalculator"; }
    const char* Icon() const override { return ICON_FA_GHOST; }

    void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    void SignalTerminate() override;
    void Update(float) override;
    void DrawSettings() override;
    void Draw(IDirect3DDevice9* pDevice) override;
private:
    void resetPredictions();

    int64_t damageFinishPrediction = 0u;
    int64_t restFinishPrecition = 0u;
    int64_t missingDamagePrediction = 0;
    bool terminating = false;
};
