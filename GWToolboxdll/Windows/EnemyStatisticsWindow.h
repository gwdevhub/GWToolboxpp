#pragma once

#include <ToolboxWindow.h>

class EnemyStatisticsWindow : public ToolboxWindow {
public:
    static EnemyStatisticsWindow& Instance()
    {
        static EnemyStatisticsWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Enemy Statistics"; }
    [[nodiscard]] const char* Description() const override { return "Displays incoming damage and enemy skill statistics."; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_SKULL_CROSSBONES; }

    void Initialize() override;
    void Terminate() override;

    void Draw(IDirect3DDevice9* pDevice) override;
    void Update(float delta) override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;
};
