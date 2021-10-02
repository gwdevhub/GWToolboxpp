#pragma once

#include <Color.h>
#include <ToolboxWidget.h>

class EffectsMonitorWidget final : public ToolboxWidget
{
    EffectsMonitorWidget()
    {
        is_resizable = false;
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


    // Draw user interface. Will be called every frame if the element is visible
    void Draw(IDirect3DDevice9 *pDevice) override;

    void Update(float delta) override;

    static void OnEffectUIMessage(GW::HookStatus*, uint32_t message_id, void* wParam, void* lParam);


private:

    // Overall settings
    enum Layout
    {
        Rows,
        Columns
    };
    Layout layout = Layout::Rows;
    const float default_skill_width = 52.f;
    float m_skill_width = 52.f;
    bool map_ready = false;
    bool initialised = false;

    std::map<uint32_t,std::vector<GW::Effect>> cached_effects;

    size_t GetEffectIndex(const std::vector<GW::Effect>& arr, uint32_t skill_id);
    void SetEffect(const GW::Effect* effect, bool renew = false);
    const GW::Effect* GetEffect(uint32_t effect_id);
    void RemoveEffect(uint32_t skill_id);
    void Refresh();

    uint32_t GetEffectType(uint32_t skill_id);

    GW::HookEntry OnEffect_Entry;

    bool snap_to_gw_interface = true;

};
