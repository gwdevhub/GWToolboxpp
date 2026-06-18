#pragma once

#include <ToolboxWindow.h>

class InfoWindow : public ToolboxWindow {
    InfoWindow() = default;
    ~InfoWindow() override = default;

public:
    static InfoWindow& Instance()
    {
        static InfoWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Info"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_INFO_CIRCLE; }

    struct Settings {
        bool show_widgets = true;
        bool show_open_chest = true;
        bool show_player = true;
        bool show_target = true;
        bool show_map = true;
        bool show_dialog = true;
        bool show_item = true;
        bool show_mobcount = true;
        bool show_quest = true;
        bool show_resignlog = true;
    };

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Terminate() override;
    void SignalTerminate() override;

    void Draw(IDirect3DDevice9* pDevice) override;

    void DrawSettingsInternal() override;
};
