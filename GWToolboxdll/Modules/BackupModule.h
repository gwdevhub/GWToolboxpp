#pragma once

#include <ToolboxModule.h>

class BackupModule : public ToolboxModule {
    BackupModule() = default;
    ~BackupModule() override = default;

public:
    static BackupModule& Instance()
    {
        static BackupModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Backup"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_SAVE; }
    [[nodiscard]] const char* Description() const override { return "Create and restore ZIP archives of GWToolbox settings files"; }

    void Initialize() override;
    void DrawSettingsInternal() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;

    // Creates a text-only backup in <settings_folder>/backups/. Called before toolbox updates.
    static bool CreateAutoBackup();
};
