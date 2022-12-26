#pragma once

#include <GWCA\Utilities\Hook.h>

#include <GWCA\Packets\StoC.h>

#include <Utils/GuiUtils.h>
#include <ToolboxWindow.h>
#include <Modules/HallOfMonumentsModule.h>

class InfoWindow : public ToolboxWindow {
    InfoWindow() = default;
    ~InfoWindow() = default;

public:
    static InfoWindow& Instance() {
        static InfoWindow instance;
        return instance;
    }

    const char* Name() const override { return "Info"; }
    const char* Icon() const override { return ICON_FA_INFO_CIRCLE; }

    void Initialize() override;
    void Terminate() override;

    void RegisterSettingsContent() override;

    void Draw(IDirect3DDevice9* pDevice) override;
    void Update(float delta) override;

    void DrawSettingInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
};
