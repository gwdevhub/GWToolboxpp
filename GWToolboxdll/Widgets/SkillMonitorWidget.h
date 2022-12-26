#pragma once
#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Constants/Skills.h>

#include <GWCA/Managers/UIMgr.h>

#include <Color.h>
#include <Timer.h>
#include <ToolboxWidget.h>

class SkillMonitorWidget : public ToolboxWidget {
    SkillMonitorWidget() = default;
    ~SkillMonitorWidget() = default;

public:
    static SkillMonitorWidget& Instance() {
        static SkillMonitorWidget instance;
        return instance;
    }

    const char* Name() const override { return "Skill Monitor"; }
    const char* Icon() const override { return ICON_FA_HISTORY; }

    void Initialize() override;
    void Terminate() override;

    void Draw(IDirect3DDevice9* device) override;
    void Update(float delta) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;

private:
    enum SkillActivationStatus {
        CASTING,
        COMPLETED,
        CANCELLED,
        INTERRUPTED,
    };

    struct SkillActivation {
        GW::Constants::SkillID id;
        SkillActivationStatus status;
        clock_t last_update;
        clock_t cast_start = last_update;
        float cast_time = .0f;
    };

    const float PARTY_OFFSET_LEFT_BASE = 15.f;
    const float PARTY_OFFSET_TOP_BASE = 31.f;
    const float PARTY_OFFSET_RIGHT_BASE = 14.f;
    const float PARTY_MEMBER_PADDING_FIXED = 1.f;
    const float PARTY_HERO_INDENT_BASE = 22.f;

    Color GetColor(SkillActivationStatus status) {
        switch (status) {
            case CASTING: return status_color_casting;
            case COMPLETED: return status_color_completed;
            case CANCELLED: return status_color_cancelled;
            case INTERRUPTED: return status_color_interrupted;
        }
        return Colors::Empty();
    }

    void SkillCallback(uint32_t value_id, uint32_t caster_id, uint32_t value);
    void CasttimeCallback(uint32_t value_id, uint32_t caster_id, float value);

    GW::UI::WindowPosition* party_window_position = nullptr;

    GW::HookEntry InstanceLoadInfo_Entry;
    GW::HookEntry GenericValueSelf_Entry;
    GW::HookEntry GenericValueTarget_Entry;
    GW::HookEntry GenericModifier_Entry;

    std::unordered_map<GW::AgentID, std::vector<SkillActivation>> history;
    std::unordered_map<GW::AgentID, float> casttime_map;

    std::unordered_map<GW::AgentID, size_t> party_map;
    std::unordered_map<GW::AgentID, bool> party_map_indent;
    size_t allies_start = 255;
    bool FetchPartyInfo();

    bool hide_in_outpost = false;
    bool show_non_party_members = false;
    Color background = Colors::ARGB(76, 0, 0, 0);

    bool snap_to_party_window = true;
    int user_offset = -1;
    int row_height = GW::Constants::HealthbarHeight::Normal;

    bool history_flip_direction = false;

    int cast_indicator_threshold = 1000;
    int cast_indicator_height = 3;
    Color cast_indicator_color = Colors::ARGB(255, 55, 153, 30);

    int status_border_thickness = 2;
    Color status_color_completed = Colors::ARGB(255, 219, 157, 14);
    Color status_color_casting = Colors::ARGB(255, 55, 153, 30);
    Color status_color_cancelled = Colors::ARGB(255, 71, 24, 102);
    Color status_color_interrupted = Colors::ARGB(255, 71, 24, 102);

    int history_length = 5;
    int history_timeout = 5000;
};
