#pragma once

#include <Widgets/SnapsToPartyWindow.h>

class SkillMonitorWidget : public SnapsToPartyWindow {
protected:
    static void OnStoCPacket(GW::HookStatus* status, GW::Packet::StoC::PacketBase* base);
    static void SkillCallback(const uint32_t value_id, const uint32_t caster_id, const uint32_t value);
public:
    static SkillMonitorWidget& Instance()
    {
        static SkillMonitorWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Skill Monitor"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_HISTORY; }

    struct Settings {
        bool hide_in_outpost = false;
        bool show_non_party_members = false;
        Colors::SettingColor background = Colors::ARGB(76, 0, 0, 0);

        int user_offset = -1;

        bool history_flip_direction = false;

        int cast_indicator_threshold = 1000;
        int cast_indicator_height = 3;
        Colors::SettingColor cast_indicator_color = Colors::ARGB(255, 55, 153, 30);

        int status_border_thickness = 2;
        Colors::SettingColor status_color_completed = Colors::ARGB(255, 219, 157, 14);
        Colors::SettingColor status_color_casting = Colors::ARGB(255, 55, 153, 30);
        Colors::SettingColor status_color_cancelled = Colors::ARGB(255, 71, 24, 102);
        Colors::SettingColor status_color_interrupted = Colors::ARGB(255, 71, 24, 102);

        bool overlay_party_window = false;

        int history_length = 5;
        int history_timeout = 5000;
    };

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void Terminate() override;

    void Draw(IDirect3DDevice9* device) override;
    void Update(float delta) override;

    void DrawSettingsInternal() override;
};
