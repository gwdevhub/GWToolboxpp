#pragma once

#include <ToolboxModule.h>

class BackupModule : public ToolboxModule {
    BackupModule() = default;
    ~BackupModule() override = default;

public:
    
    struct Settings {
        bool backup_text_files  = true;
        bool backup_image_files = false;
        bool backup_audio_files = false;
    } settings;
    
    
    static BackupModule& Instance()
    {
        static BackupModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "备份模块"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_SAVE; }
    [[nodiscard]] const char* Description() const override { return "创建和恢复GWToolbox设置文件的ZIP文件。"; }

    void Initialize() override;
    void DrawSettingsInternal() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;

    // Creates a text-only backup in <settings_folder>/backups/. Called before toolbox updates.
    static bool CreateAutoBackup();
};
