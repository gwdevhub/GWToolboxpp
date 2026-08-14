#pragma once

#include <ToolboxWindow.h>

class TargetInfoWindow : public ToolboxWindow {
public:
    static TargetInfoWindow& Instance()
    {
        static TargetInfoWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "目标信息"; }
    [[nodiscard]] const char* Description() const override { return "显示有关当前目标的信息，包括GWW的全部注释"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_CROSSHAIRS; }

    struct Settings {
        bool auto_hide = true;
    };

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Terminate() override;

    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingsInternal() override;
    void Update(float) override;
};
