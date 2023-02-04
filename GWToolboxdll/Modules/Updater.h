#pragma once

#include "ToolboxUIElement.h"

class Updater : public ToolboxUIElement {
    Updater() { can_show_in_main_window = false; };
    ~Updater() = default;

public:
    static Updater& Instance() {
        static Updater instance;
        return instance;
    }

    const char* Name() const override { return "Updater"; }
    // DrawSettingInternal() called via ToolboxSettings; don't draw it again
    bool HasSettings() override { return false;  }

    void RegisterSettingsContent() override {
        ToolboxModule::RegisterSettingsContent();
    }
    void CheckForUpdate(const bool forced = false);

    void Draw(IDirect3DDevice9* device) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;
    
    static const std::string& GetServerVersion();
};
