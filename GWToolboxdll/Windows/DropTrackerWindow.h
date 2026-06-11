#pragma once

#include <ToolboxWindow.h>

class DropTrackerWindow : public ToolboxWindow {
    DropTrackerWindow() = default;
    ~DropTrackerWindow() override = default;

public:
    static DropTrackerWindow& Instance()
    {
        static DropTrackerWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Drop Tracker"; }
    [[nodiscard]] const char* Description() const override { return "Shows you what drops you've gotten"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_COINS; }

    struct Settings {
        float icon_size = 48;
        float run_count = 0;
    };

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Update(float) override {}

    void Draw(IDirect3DDevice9* pDevice) override;

    void DrawSettingsInternal() override;
};
