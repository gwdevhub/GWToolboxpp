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

    void Terminate() override;

    void Update(float) override {}

    void Draw(IDirect3DDevice9* pDevice) override;

    void DrawSettingsInternal() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
};
