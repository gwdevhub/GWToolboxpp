#pragma once

#include <optional>

#include <ToolboxUIElement.h>

struct GWToolboxRelease {
    std::string body;
    std::string version;
    std::string download_url;
    uintmax_t size = 0;
    uintmax_t asset_size = 0;
    std::optional<std::string> digest;
    bool prerelease = false;
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
    [[nodiscard]] const char* SettingsName() const override { return "Toolbox Settings"; }
    bool HasSettings() override { return false; }

    enum class ReleaseType : int {
        Stable,
        Beta
    };
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
    static bool CheckBeforeInitialize(HMODULE module);
    static bool IsLatestVersion();

    static const GWToolboxRelease* GetCurrentVersionInfo(GWToolboxRelease* out, HMODULE module = nullptr);

    void Initialize() override;
    void Draw(IDirect3DDevice9* device) override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;

    static const std::string& GetServerVersion();
};
