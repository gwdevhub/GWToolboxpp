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

    void Initialize() override;
    void Terminate() override;

    void Draw(IDirect3DDevice9* pDevice) override;
    void Update(float delta) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
};
