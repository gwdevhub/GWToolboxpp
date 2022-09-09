#pragma once

#include "ToolboxUIElement.h"

class Updater : public ToolboxUIElement {
    Updater() { can_show_in_main_window = false; };
    ~Updater() {};
public:
    static Updater& Instance() {
        static Updater instance;
        return instance;
    }
    struct GWToolboxRelease {
        std::string body;
        std::string version;
        std::string download_url;
    };
    const char* Name() const override { return "Updater"; }
    // DrawSettingInternal() called via ToolboxSettings; don't draw it again
    bool HasSettings() override { return false;  }

    void RegisterSettingsContent() override {
        ToolboxModule::RegisterSettingsContent();
    }
    void CheckForUpdate(const bool forced = false);
    void DoUpdate();

    void Draw(IDirect3DDevice9* device) override;

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void Initialize() override;
    void DrawSettingInternal() override;
    void GetLatestRelease(GWToolboxRelease*) const;
    std::string GetServerVersion() const { return latest_release.version; }

private:
    GWToolboxRelease latest_release;

    // 0=none, 1=check and warn, 2=check and ask, 3=check and do
    enum class Mode : uint8_t {
        DontCheckForUpdates,
        CheckAndWarn,
        CheckAndAsk,
        CheckAndAutoUpdate
    } mode = Mode::CheckAndAsk;

    // 0=checking, 1=asking, 2=downloading, 3=done
    enum Step {
        Checking,
        NewVersionAvailable,
        Downloading,
        Success,
        Done
    } step = Step::Checking;
    bool notified = false;
    bool forced_ask = false;
    clock_t last_check = 0;
};
