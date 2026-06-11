#pragma once

#include <Widgets/SnapsToPartyWindow.h>

namespace GW {
    namespace Constants {
        enum class SkillID : uint32_t;
    }
}

class BondsWidget : public SnapsToPartyWindow {
protected:
    bool DrawBondImage(uint32_t agent_id, GW::Constants::SkillID skill_id, ImVec2* top_left_out, ImVec2* bottom_right_out);
    bool GetBondPosition(uint32_t agent_id, GW::Constants::SkillID skill_id, ImVec2* top_left_out, ImVec2* bottom_right_out);
public:
    static BondsWidget& Instance()
    {
        static BondsWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Bonds"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_BARS; }

    struct Settings {
        Colors::SettingColor background = Colors::ARGB(76, 0, 0, 0);
        Colors::SettingColor low_attribute_overlay = Colors::ARGB(76, 0, 0, 0);
        bool click_to_cast = true;
        bool click_to_drop = true;
        bool show_allies = true;
        bool flip_bonds = false;
        bool hide_in_outpost = false;
        // Distance away from the party window on the x axis; used with snap to party window
        int user_offset = 64;
        bool overlay_party_window = false;
    };

    void Initialize() override;
    void Terminate() override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* device) override;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;

    static bool IsBondLikeSkill(GW::Constants::SkillID skill_id);
};
