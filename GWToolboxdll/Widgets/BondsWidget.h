#pragma once

#include <ToolboxWidget.h>

class BondsWidget : public ToolboxWidget {
    BondsWidget() = default;
    ~BondsWidget() override = default;

public:
    static BondsWidget& Instance()
    {
        static BondsWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Bonds"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_BARS; }

    void Initialize() override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* device) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
};
