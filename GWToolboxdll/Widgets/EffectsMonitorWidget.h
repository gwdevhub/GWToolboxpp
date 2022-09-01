#pragma once

#include <Color.h>
#include <ToolboxWidget.h>

class EffectsMonitorWidget : public ToolboxWidget
{
    EffectsMonitorWidget();
public:
    static EffectsMonitorWidget& Instance();

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
