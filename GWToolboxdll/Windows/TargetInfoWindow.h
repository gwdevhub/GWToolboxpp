#pragma once

#include <ToolboxWindow.h>

class TargetInfoWindow : public ToolboxWindow {
public:
    static TargetInfoWindow& Instance()
    {
        static TargetInfoWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Target Info"; }
    [[nodiscard]] const char* Description() const override { return "Displays info about current target including any notes from GWW"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_CROSSHAIRS; }

    void Initialize() override;
    void Terminate() override;

    void Draw(IDirect3DDevice9* pDevice) override;
};
