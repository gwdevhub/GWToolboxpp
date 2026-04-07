#pragma once

#include <ToolboxModule.h>

class FavorTrackerModule : public ToolboxModule {
    FavorTrackerModule() = default;
    ~FavorTrackerModule() override = default;

public:
    static FavorTrackerModule& Instance()
    {
        static FavorTrackerModule instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Favor Tracker"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_PRAY; }
    [[nodiscard]] const char* Description() const override { return "Tracks Favor of the Gods status"; }

    void Initialize() override;
    void Update(float delta) override;
    void SignalTerminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    // Returns remaining favor minutes, or 0 if no favor
    static uint32_t GetFavorMinutes();

    // Returns an encoded string of the current favor
    static const wchar_t* GetFavorMessageW();
    static const char* GetFavorMessage();

    // Returns true if favor is currently active
    static bool HasFavor();
};
