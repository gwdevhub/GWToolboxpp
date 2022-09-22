#pragma once

#include <ToolboxWidget.h>

class EffectsMonitorWidget : public ToolboxWidget
{
    EffectsMonitorWidget() { is_movable = is_resizable = false; }
    ~EffectsMonitorWidget() = default;

public:
    static EffectsMonitorWidget& Instance()
    {
        static EffectsMonitorWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override
    {
        return "Effect Durations";
    }
    [[nodiscard]] const char8_t* Icon() const override { return ICON_FA_HISTORY; }

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;
    void Initialize() override;
    void Terminate() override;
    void Draw(IDirect3DDevice9 *pDevice) override;
};
