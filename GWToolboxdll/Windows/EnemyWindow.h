#pragma once

#include <GWCA/Constants/Constants.h>
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

    struct Settings {
        bool show_enemy_level = true;
        bool show_enemy_last_skill = true;
        float enemies_threshhold = 1.f;
        float range = GW::Constants::Range::Spellcast;
        float triangle_spacing = 22.f;
        float last_skill_threshold = 3000.f;
    };

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Terminate() override;

    // Update. Will always be called every frame.
    void Update(float) override {}

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void DrawSettingsInternal() override;
};
