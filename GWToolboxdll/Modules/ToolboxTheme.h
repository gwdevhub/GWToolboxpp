#pragma once

#include <ToolboxModule.h>

class ToolboxTheme : public ToolboxModule {
    ToolboxTheme();
public:
    static ToolboxTheme& Instance() {
        static ToolboxTheme instance;
        return instance;
    }

    const char* Name() const override { return "Theme"; }
    const char* Icon() const override { return ICON_FA_PALETTE; }

    void Terminate() override;
    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;

    void PreloadWindowLayouts();
    void SaveUILayout(const char* layout_name = nullptr);
    void LoadUILayout(const char* layout_name = nullptr);

    void DrawSettingInternal() override;

private:
    ImGuiStyle DefaultTheme();

    ImGuiStyle ini_style;
    CSimpleIni* inifile = nullptr;
    CSimpleIni* window_layouts_ini = nullptr;
};
