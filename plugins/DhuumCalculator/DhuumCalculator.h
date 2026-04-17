#pragma once

#include <ToolboxUIPlugin.h>

#include <IconsFontAwesome5.h>

class DhuumCalculator : public ToolboxUIPlugin {
public:
    DhuumCalculator() {}
    ~DhuumCalculator() override = default;

    const char* Name() const override { return "DhuumCalculator"; }
    const char* Icon() const override { return ICON_FA_GHOST; }

    void Update(float) override;
    void DrawSettings() override;
    void Draw(IDirect3DDevice9* pDevice) override;
private:
    int64_t damageFinishPrediction = 0u;
    int64_t restFinishPrecition = 0u;
    int64_t missingDamagePrediction = 0;
};
