#pragma once

#include <Color.h>
#include <ToolboxWidget.h>

class HealthWidget : public ToolboxWidget {
    HealthWidget() = default;
    ~HealthWidget() {
        for (const auto* threshold : thresholds) {
            delete threshold;
        }
        thresholds.clear();
    }

public:
    static HealthWidget& Instance() {
        static HealthWidget instance;
        return instance;
    }

    const char* Name() const override { return "Health"; }
    const char* Icon() const override { return ICON_FA_PERCENTAGE; }

    void LoadSettings(ToolboxIni *ini) override;
    void SaveSettings(ToolboxIni *ini) override;
    void DrawSettingInternal() override;

    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9* pDevice) override;

    bool click_to_print_health = false;

    std::wstring agent_name_ping;
    bool hide_in_outpost = false;
    bool show_abs_value = true;
    bool show_perc_value = true;

    bool thresholds_changed = false;
private:
    class Threshold {
        private:
            static unsigned int cur_ui_id;
        public:
            enum class Operation {
                None,
                MoveUp,
                MoveDown,
                Delete,
            };

            Threshold(ToolboxIni* ini, const char* section);
            Threshold(const char* _name, Color _color, int _value);

            bool DrawHeader();
            bool DrawSettings(Operation& op);
            void SaveSettings(ToolboxIni* ini, const char* section);

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
    ToolboxIni* inifile = nullptr;
};
