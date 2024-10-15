#pragma once

#include <ToolboxWidget.h>

class MissionMapWidget : public ToolboxWidget {
    MissionMapWidget() = default;
    ~MissionMapWidget() override = default;

public:
    static MissionMapWidget& Instance()
    {
        static MissionMapWidget w;
        return w;
    }
    bool HasSettings() override { return false; }
    [[nodiscard]] const char* Name() const override { return "Mission Map"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_GLOBE; }

    void Draw(IDirect3DDevice9* pDevice) override;
};
