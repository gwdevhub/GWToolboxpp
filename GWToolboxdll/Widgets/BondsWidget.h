#pragma once

#include <ToolboxWidget.h>

namespace GW {
    namespace Constants {
        enum class SkillID : uint32_t;
    }
}

class BondsWidget : public ToolboxWidget {
    BondsWidget() = default;
    ~BondsWidget() override = default;

public:
    static BondsWidget& Instance()
    {
        static BondsWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Bonds"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_BARS; }

    void Initialize() override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* device) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    static bool UseBuff(GW::AgentID targetId, GW::Constants::SkillID skill_id);
    static bool DropBuffs(GW::AgentID targetId = (GW::AgentID)0, GW::Constants::SkillID skill_id = (GW::Constants::SkillID)0);
    static bool IsBondLikeSkill(GW::Constants::SkillID skill_id);
};
