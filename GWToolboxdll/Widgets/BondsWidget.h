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

    void Initialize() override;
    void Terminate() override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* device) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    static bool IsBondLikeSkill(GW::Constants::SkillID skill_id);
};
