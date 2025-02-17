#pragma once

#include <ToolboxWidget.h>

class DistanceWidget : public ToolboxWidget {
    DistanceWidget()
    {
        is_resizable = false;
        auto_size = true;
    }
    ~DistanceWidget() override = default;

public:
    static DistanceWidget& Instance()
    {
        static DistanceWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Distance"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_RULER; }

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
};
