#pragma once

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

    struct Settings {
        bool show_in_outpost = true;
        bool show_in_explorable = true;
    };

    void DrawSettingsInternal() override;
    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void SignalTerminate() override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;
};
