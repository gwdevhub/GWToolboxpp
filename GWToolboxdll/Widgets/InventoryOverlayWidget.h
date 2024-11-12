#pragma once

#include <Color.h>
#include <ToolboxWidget.h>

class InventoryOverlayWidget : public ToolboxWidget {
public:
    static InventoryOverlayWidget& Instance()
    {
        static InventoryOverlayWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Inventory Overlay"; }
    [[nodiscard]] const char* Description() const override { return "Draws over the top of inventory slots in-game to better identify items"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_TH; }

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
    void Initialize() override;
    void SignalTerminate() override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;
};
