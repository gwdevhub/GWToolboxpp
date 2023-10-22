#pragma once

#include <ToolboxWidget.h>

class VanquishWidget : public ToolboxWidget {
    VanquishWidget() = default;
    ~VanquishWidget() override = default;

public:
    static VanquishWidget& Instance()
    {
        static VanquishWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Vanquish"; }

    [[nodiscard]] const char* Icon() const override { return ICON_FA_SKULL; }

    void Draw(IDirect3DDevice9* pDevice) override;

    void DrawSettingsInternal() override;
};
