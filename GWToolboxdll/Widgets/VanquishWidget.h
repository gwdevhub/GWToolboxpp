#pragma once

#include <ToolboxWidget.h>

class VanquishWidget : public ToolboxWidget {
    VanquishWidget() = default;
    ~VanquishWidget() = default;

public:
    static VanquishWidget& Instance() {
        static VanquishWidget instance;
        return instance;
    }

    const char* Name() const override { return "Vanquish"; }

    const char* Icon() const override { return ICON_FA_SKULL;  }

    void Draw(IDirect3DDevice9* pDevice) override;

    void DrawSettingInternal() override;
};
