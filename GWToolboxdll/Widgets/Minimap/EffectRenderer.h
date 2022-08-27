#pragma once

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Packets/StoC.h>
#include <GWCA/Managers/UIMgr.h>

#include <Color.h>
#include <Timer.h>

#include <Widgets/Minimap/VBuffer.h>

class EffectRenderer : public VBuffer {
    friend class Minimap;

    const float drawing_scale = 96.0f;

    class EffectCircle : public VBuffer {
        void Initialize(IDirect3DDevice9* device) override;
    public:
        Color* color = nullptr;
        float range = GW::Constants::Range::Adjacent;
    };
    struct Effect {
        Effect(uint32_t _effect_id, float _x, float _y, uint32_t _duration,
               float range, Color *_color)
            : start(TIMER_INIT())
            , effect_id(_effect_id)
            , pos(_x,_y)
            , duration(_duration)
        {
            circle.range = range;
            circle.color = _color;
        };
        clock_t start;
        const uint32_t effect_id;
        const GW::Vec2f pos;
        uint32_t duration;
        EffectCircle circle;
    };

public:

    void Render(IDirect3DDevice9* device) override;

    void Invalidate() override;
    EffectRenderer();
    EffectRenderer(const EffectRenderer&) = delete;
    ~EffectRenderer();
    void PacketCallback(GW::Packet::StoC::GenericValue* pak);
    void PacketCallback(GW::Packet::StoC::GenericValueTarget* pak);
    void PacketCallback(GW::Packet::StoC::PlayEffect* pak);

    void LoadDefaults();
    void DrawSettings();
    void LoadSettings(CSimpleIni* ini, const char* section);
    void SaveSettings(CSimpleIni* ini, const char* section) const;

private:
    void Initialize(IDirect3DDevice9* device) override;

    void RemoveTriggeredEffect(uint32_t effect_id, GW::Vec2f* pos);
    void DrawAoeEffects(IDirect3DDevice9* device);
    std::vector<Effect*> aoe_effects;
    struct pair_hash
    {
        template <class T1, class T2>
        std::size_t operator() (const std::pair<T1, T2>& pair) const
        {
            return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
        }
    };
    bool need_to_clear_effects = false;

    std::recursive_mutex effects_mutex;

    struct EffectSettings {
        Color color = 0xFFFF0000;
        std::string name;
        uint32_t effect_id;
        float range = GW::Constants::Range::Nearby;
        uint32_t stoc_header = 0;
        uint32_t duration = 10000;
        EffectSettings(const char* _name, uint32_t _effect_id, float _range, uint32_t _duration, uint32_t _stoc_header = 0)
            : name(_name)
            , effect_id(_effect_id)
            , range(_range)
            , stoc_header(_stoc_header)
            , duration(_duration)
        {
        }
    };
    struct EffectTrigger {
        uint32_t triggered_effect_id = 0;
        uint32_t duration = 2000;
        float range = GW::Constants::Range::Nearby;
        std::unordered_map<std::pair<float, float>, clock_t, pair_hash> triggers_handled;
        EffectTrigger(uint32_t _triggered_effect_id, uint32_t _duration, float _range) : triggered_effect_id(_triggered_effect_id), duration(_duration), range(_range) {};
    };

    std::unordered_map<uint32_t, EffectSettings*> aoe_effect_settings;
    std::unordered_map<uint32_t, EffectTrigger*> aoe_effect_triggers;

    GW::HookEntry StoC_Hook;

    unsigned int vertices_max = 0x1000; // max number of vertices to draw in one call
};
