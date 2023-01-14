#pragma once

#include <ToolboxWindow.h>

class ArmoryWindow : public ToolboxWindow {
public:
    static ArmoryWindow& Instance()
    {
        static ArmoryWindow instance;
        return instance;
    }

    const char* Name() const override { return "GwArmory"; }
    const char* Icon() const override { return ICON_FA_VEST; }

    void Initialize() override;
    void Terminate() override;

    void Draw(IDirect3DDevice9* pDevice) override;
    void Update(float delta) override;
};
