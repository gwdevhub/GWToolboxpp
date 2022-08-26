#pragma once

#include <ToolboxUIElement.h>

class ToolboxTheme : public ToolboxUIElement {
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
    const char8_t* Icon() const override { return ICON_FA_PALETTE; }

    void Terminate() override;
    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void Draw(IDirect3DDevice9* device) override;
    void ShowVisibleRadio() override {}
    void DrawSizeAndPositionSettings() override {}

    void SaveUILayout();
    void LoadUILayout();

    void DrawSettingInternal() override;

    CSimpleIni* GetLayoutIni();

private:
    ImGuiStyle DefaultTheme();

    float font_global_scale = 1.0;
    ImGuiStyle ini_style;
    bool layout_dirty = false;

    CSimpleIni* inifile = nullptr;
    CSimpleIni* windows_ini = nullptr;
};
