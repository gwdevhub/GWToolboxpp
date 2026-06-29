#pragma once

#include <ToolboxWidget.h>

class ClockWidget : public ToolboxWidget {
    ClockWidget() = default;
    ~ClockWidget() override = default;

public:
    static ClockWidget& Instance()
    {
        static ClockWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Clock"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_CLOCK; }

    // MSVC can't reflect member names of internal-linkage types, so settings structs are nested in the class
    struct Settings {
        bool use_24h_clock = true;
        bool show_seconds = false;
        float font_size = 48.0f;
    };

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;
};
