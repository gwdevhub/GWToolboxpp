#pragma once

#include <ToolboxModule.h>

class ToolboxTheme : public ToolboxModule {
    ToolboxTheme();
    ~ToolboxTheme() { 
        if (windows_ini) delete windows_ini;
        if (inifile) delete inifile;
    }
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

    void SaveUILayout();
    void LoadUILayout();

    void DrawSettingInternal() override;

    CSimpleIni* GetLayoutIni();

private:
    ImGuiStyle DefaultTheme();

    ImGuiStyle ini_style;

    CSimpleIni* inifile = nullptr;
    CSimpleIni* windows_ini = nullptr;
};
