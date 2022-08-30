#pragma once

#include <Defines.h>
#include <ToolboxWidget.h>

class ClockWidget : public ToolboxWidget {
    ClockWidget() = default;
    ~ClockWidget() = default;
public:
    static ClockWidget& Instance() {
        static ClockWidget instance;
        return instance;
    }

    const char* Name() const override { return "Clock"; }
    const char8_t* Icon() const override { return ICON_FA_CLOCK; }

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;

private:
    bool use_24h_clock = true;
    bool show_seconds = false;
};
