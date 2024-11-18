#pragma once

#include <Color.h>
#include <ToolboxWidget.h>

#include "Utils/FontLoader.h"

namespace FontLoader {
    enum class FontSize;
}

class SkillbarWidget final : public ToolboxWidget {
    SkillbarWidget()
    {
        is_movable = is_resizable = false;
    }

    ~SkillbarWidget() override = default;

public:
    static SkillbarWidget& Instance()
    {
        static SkillbarWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override
    {
        return "Skillbar";
    }

    [[nodiscard]] const char* Icon() const override { return ICON_FA_HISTORY; }

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
    void Initialize() override;
    void Terminate() override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    void Update(float delta) override;

private:
    void DrawEffect(int i, const ImVec2& pos) const;

    struct Effect {
        float progress = 1.0f;  // 1 to 0
        uint32_t remaining = 0; // in ms
        char text[16] = {0};
        Color color{};
    };

    struct Skill {
        char cooldown[16]{};
        Color color{};
        std::vector<Effect> effects{};
    };

    std::array<Skill, 8> m_skills{};

    // Internal utils
    [[nodiscard]] Color UptimeToColor(uint32_t uptime) const;
    static std::vector<Effect> get_effects(GW::Constants::SkillID skillId);
    static Effect get_longest_effect(GW::Constants::SkillID skillId);
    void skill_cooldown_to_string(char arr[16], uint32_t cd) const;
    void DrawDurationThresholds();
};
