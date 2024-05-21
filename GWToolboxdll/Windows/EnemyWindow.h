#pragma once

#include <ToolboxWindow.h>

class EnemyWindow : public ToolboxWindow {
    EnemyWindow() = default;
    ~EnemyWindow() override = default;

public:
    static EnemyWindow& Instance()
    {
        static EnemyWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Enemy Window"; }
    [[nodiscard]] const char* Description() const override { return "Keeps track of enemies"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_COPY; }

    void Terminate() override;

    // Update. Will always be called every frame.
    void Update(float) override {}

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
};
