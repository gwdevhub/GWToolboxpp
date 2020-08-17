#pragma once

#include <Color.h>
#include <ToolboxWidget.h>

class SkillbarWidget final : public ToolboxWidget
{
    SkillbarWidget()
    {
        is_resizable = false;
    };
    ~SkillbarWidget(){};

public:
    static SkillbarWidget &Instance()
    {
        static SkillbarWidget instance;
        return instance;
    }

    [[nodiscard]] const char *Name() const override
    {
        return "Skillbar";
    }

    void LoadSettings(CSimpleIni *ini) override;
    void SaveSettings(CSimpleIni *ini) override;
    void DrawSettingInternal() override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9 *pDevice) override;

    void Update(float delta) override;

private:
    void DrawEffect(int i, const ImVec2& pos) const;

    struct Effect
    {
        float progress = 1.0f; // 1 to 0
        uint32_t remaining = 0; // in ms
        std::array<char, 16> text{};
        Color color{};
    };
    struct Skill
    {
        std::array<char, 16> cooldown{};
        Color color{};
        std::vector<Effect> effects;
    };
    std::array<Skill, 8> m_skills{};

    // Overall settings
    bool vertical = false;
    int m_skill_width = 50;
    int m_skill_height = 50;

    // duration -> color settings
    int medium_treshold = 5000; // long to medium color
    int short_treshold = 2500;  // medium to short color
    Color color_long = Colors::ARGB(50, 0, 255, 0);
    Color color_medium = Colors::ARGB(50, 255, 255, 0);
    Color color_short = Colors::ARGB(80, 255, 0, 0);

    // duration -> string settings
    int decimal_threshold = 600; // when to start displaying decimals
    bool round_up = true;        // round up or down?

    // Skill overlay settings
    bool display_skill_overlay = true;
    GuiUtils::FontSize font_recharge = GuiUtils::FontSize::f20;
    Color color_text_recharge = Colors::White();
    Color color_border = Colors::ARGB(100, 255, 255, 255);

    // Effect monitor settings
    bool display_effect_monitor = false;
    int effect_monitor_offset = -100;
    bool display_multiple_effects = false;
    bool effect_text_color = false;
    bool effect_progress_bar_color = false;
    GuiUtils::FontSize font_effects = GuiUtils::FontSize::f16;
    Color color_text_effects = Colors::White();
    Color color_effect_background = Colors::ARGB(100, 0, 0, 0);
    Color color_effect_border = Colors::ARGB(255, 0, 0, 0);
    Color color_effect_progress = Colors::Blue();

    // Internal utils
    Color UptimeToColor(uint32_t uptime) const;
    void skill_cooldown_to_string(std::array<char, 16>& arr, uint32_t cooldown) const;
    std::vector<Effect> get_effects(const uint32_t skillId) const;
    Effect get_longest_effect(const uint32_t skillId) const;
};
