#pragma once

#include <ToolboxUIElement.h>

struct GWToolboxRelease {
    std::string body;
    std::string version;
    std::string download_url;
    uintmax_t size = 0;
};

class Updater : public ToolboxUIElement {
    Updater() { can_show_in_main_window = false; };
    ~Updater() override = default;

public:
    static Updater& Instance()
    {
        static Updater instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Updater"; }
    // DrawSettingInternal() called via ToolboxSettings; don't draw it again
    bool HasSettings() override { return false; }

    enum class ReleaseType : int {
        Stable,
        Beta
    };
    // 0=none, 1=check and warn, 2=check and ask, 3=check and do
    enum class Mode : int {
        DontCheckForUpdates,
        CheckAndWarn,
        CheckAndAsk,
        CheckAndAutoUpdate
    };

    struct Settings {
        Mode update_mode = Mode::CheckAndAsk;
        ReleaseType update_release_type = ReleaseType::Stable;
        bool has_starred = false;
    };

    void RegisterSettingsContent() override
    {
        ToolboxModule::RegisterSettingsContent();
    }

    static void CheckForUpdate(bool forced = false);
    static bool IsLatestVersion();

    static const GWToolboxRelease* GetCurrentVersionInfo(GWToolboxRelease* out);

    void Initialize() override;
    void Draw(IDirect3DDevice9* device) override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;

    static const std::string& GetServerVersion();
};
