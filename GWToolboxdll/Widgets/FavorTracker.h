#pragma once

#include <ToolboxWidget.h>

struct IDirect3DDevice9;
class FavorTracker : public ToolboxWidget {
    FavorTracker() = default;
    ~FavorTracker() override = default;

public:
    static FavorTracker& Instance()
    {
        static FavorTracker instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Favor Overlay"; }
    [[nodiscard]] const char* SettingsName() const override { return "Favor Tracker"; }
    [[nodiscard]] const char* Description() const override { return "Shows an on-screen overlay of the current Favor of the Gods"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_PRAY; }

    struct Settings {
        float text_size = 24.f;
        Colors::SettingColor text_color = IM_COL32_WHITE;
        bool hide_if_no_favor = false;
        bool enabled = true;
        bool play_sound_on_favor = false;
        uint32_t favor_sound_file_id = 0;
    };

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingsInternal() override;
    void Update(float delta) override;
    void SignalTerminate() override;

    static uint32_t GetFavorMinutes();
    static uint32_t GetFavorAchievementsNeeded();
    static const wchar_t* GetFavorMessageW();
    static const char* GetFavorMessage();
};
