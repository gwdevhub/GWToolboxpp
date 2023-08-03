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

    void Initialize() override;
    void Terminate() override;

    void RegisterSettingsContent() override;

    void Draw(IDirect3DDevice9* pDevice) override;
    void Update(float delta) override;

    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
};
