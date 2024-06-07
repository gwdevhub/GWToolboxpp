#pragma once

#include <ToolboxWidget.h>

class InventoryOverlayWidget : public ToolboxWidget {
    InventoryOverlayWidget() = default;
    ~InventoryOverlayWidget() override = default;

public:
    static InventoryOverlayWidget& Instance()
    {
        static InventoryOverlayWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Inventory Overlay"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_SQUARE; }

    void Initialize() override;
    void Terminate() override;
    void Draw(IDirect3DDevice9*) override;
};
