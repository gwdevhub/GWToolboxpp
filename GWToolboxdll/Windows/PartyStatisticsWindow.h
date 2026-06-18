#pragma once

#include <ToolboxWindow.h>

class PartyStatisticsWindow : public ToolboxWindow {
public:
    static PartyStatisticsWindow& Instance()
    {
        static PartyStatisticsWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Party Statistics"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_TABLE; }

    struct Settings {
        bool show_abs_values = true;
        bool show_perc_values = true;
        bool print_by_click = true;
    };

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Terminate() override;

    void Draw(IDirect3DDevice9* pDevice) override;
    void Update(float delta) override;

    void DrawSettingsInternal() override;
};
