#pragma once

#include <Color.h>
#include <ToolboxWidget.h>

class EffectsMonitorWidget final : public ToolboxWidget
{
    EffectsMonitorWidget()
    {
        is_movable = is_resizable = false;
    };
    ~EffectsMonitorWidget(){};

public:
    static EffectsMonitorWidget&Instance()
    {
        static EffectsMonitorWidget instance;
        return instance;
    }

    [[nodiscard]] const char *Name() const override
    {
        return "Effect Durations";
    }
    const char* Icon() const override { return ICON_FA_HISTORY; }

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;

    void Draw(IDirect3DDevice9 *pDevice) override;
    // Static handler for GW UI Message events. Updates ongoing effects and refreshes UI position.
    static void OnEffectUIMessage(GW::HookStatus*, uint32_t message_id, void* wParam, void* lParam);


private:

    GuiUtils::FontSize font_effects = GuiUtils::FontSize::header2;
    Color color_text_effects = Colors::White();
    Color color_background = Colors::ARGB(128, 0, 0, 0);
    Color color_text_shadow = Colors::Black();

    // duration -> string settings
    int decimal_threshold = 600; // when to start displaying decimals
    bool round_up = true;        // round up or down?
    bool show_vanquish_counter = true;
    uint32_t minion_count = 0;
    uint32_t morale_percent = 100;

    // Overall settings
    enum Layout
    {
        Rows,
        Columns
    };
    Layout layout = Layout::Rows;
    const float default_skill_width = 52.f;
    // Runtime params
    float m_skill_width = 52.f;
    float row_count = 1.f;
    float skills_per_row = 99.f;
    ImVec2 window_pos = { 0.f, 0.f };
    ImVec2 window_size = { 0.f, 0.f };
    float x_translate = 1.f; // Multiplier for traversing effects on the x axis
    float y_translate = -1.f; // Multiplier for traversing effects on the y axis
    size_t effect_count = 0;
    ImVec2 imgui_pos = { 0.f, 0.f };
    ImVec2 imgui_size = { 0.f, 0.f };

    bool map_ready = false;
    bool initialised = false;
    // Emulated effects in order of addition
    std::map<uint32_t,std::vector<GW::Effect>> cached_effects;
    // Find index of active effect from gwtoolbox overlay
    size_t GetEffectIndex(const std::vector<GW::Effect>& arr, uint32_t skill_id);
    // Update effect on the gwtoolbox overlay
    void SetEffect(const GW::Effect* effect);
    // Get matching effect from gwtoolbox overlay
    const GW::Effect* GetEffect(uint32_t effect_id);
    // Remove effect from gwtoolbox overlay
    void RemoveEffect(uint32_t skill_id);
    // Forcefully removes then re-adds the current effects; used for initialising
    void RefreshEffects();
    // Find the drawing order of the skill based on the gw effect monitor
    uint32_t GetEffectSortOrder(uint32_t skill_id);
    // Recalculate position of widget based on gw effect monitor position
    void RefreshPosition();
    // Triggered when an effect has reached < 0 duration. Returns true if redraw is needed.
    bool DurationExpired(GW::Effect& effect);
    // Adds or removes the morale "effect" depending on percent
    void SetMoralePercent(uint32_t morale_percent);
    // Adds or removes the minion count "effect" depending on percent
    void CheckSetMinionCount();

    int UptimeToString(char arr[8], uint32_t cd) const;

    GW::HookEntry OnEffect_Entry;

};
