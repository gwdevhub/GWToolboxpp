#pragma once

#include <ToolboxWindow.h>

class SettingsWindow : public ToolboxWindow {
    SettingsWindow() {
        show_menubutton = true;
    };
    ~SettingsWindow() = default;
public:
    static SettingsWindow& Instance() {
        static SettingsWindow instance;
        return instance;
    }

    const char* Name() const override { return "Settings"; }
    const char* Icon() const override { return ICON_FA_COGS; }

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    bool DrawSettingsSection(const char* section);

    size_t sep_modules = 0;
    size_t sep_windows = 0;
    size_t sep_widgets = 0;
private:
    std::map<std::string, bool> drawn_settings{};
    bool hide_when_entering_explorable = false;
};
