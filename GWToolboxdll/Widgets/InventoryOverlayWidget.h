#pragma once

#include <ToolboxWidget.h>

class InventoryOverlayWidget : public ToolboxWidget {
public:
    static InventoryOverlayWidget& Instance()
    {
        static InventoryOverlayWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "库存堆叠"; }
    [[nodiscard]] const char* Description() const override { return "在游戏中的物品栏上方绘制堆叠，以便更好地识别物品"; }
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
