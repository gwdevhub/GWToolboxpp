#pragma once

#include <ToolboxWidget.h>
#include <Utils/FontLoader.h>

class HealthWidget : public ToolboxWidget {
    HealthWidget()
    {
        is_resizable = false;
        auto_size = true;
        has_titlebar = true;
    }

    ~HealthWidget() override
    {
        for (const auto* threshold : thresholds) {
            delete threshold;
        }
        thresholds.clear();
    }

public:
    static HealthWidget& Instance()
    {
        static HealthWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Health"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_PERCENTAGE; }

    struct Settings {
        bool click_to_print_health = false;
        bool hide_in_outpost = false;
        float font_size_header = static_cast<float>(FontLoader::FontSize::header1);
        float font_size_perc_value = static_cast<float>(FontLoader::FontSize::widget_small);
        float font_size_abs_value = static_cast<float>(FontLoader::FontSize::widget_label);
    };

    struct ThresholdSettings {
        bool active = true;
        std::string name;
        int modelId = 0;
        int skillId = 0;
        int mapId = 0;
        int value = 0;
        Colors::SettingColor color = 0xFFFFFFFF;
    };

    void Initialize() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;
private:
    class Threshold {
        static unsigned int cur_ui_id;

    public:
        enum class Operation {
            None,
            MoveUp,
            MoveDown,
            Delete,
        };

        Threshold(const ToolboxIni* ini, const char* section);
        Threshold(const char* _name, Color _color, int _value);

        bool DrawHeader();
        bool DrawSettings(Operation& op);

        const unsigned int ui_id = 0;
        size_t index = 0;

        bool active = true;
        char name[128] = "";
        int modelId = 0;
        int skillId = 0;
        int mapId = 0;

        int value = 0;
        Color color = 0xFFFFFFFF;
    };
    std::vector<Threshold*> thresholds{};
};
