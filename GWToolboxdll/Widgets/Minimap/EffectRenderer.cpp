#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include <Utils/GuiUtils.h>
#include <Widgets/Minimap/EffectRenderer.h>

namespace {
    enum SkillEffect {
        Chaos_storm = 131,
        Meteor_Shower = 341,
        Savannah_heat = 346,
        Lava_font = 347,
        Breath_of_fire = 351,
        Maelstrom = 381,
        Barbed_Trap = 772,
        Barbed_Trap_Activate = 773,
        Flame_Trap = 774,
        Flame_Trap_Activate = 775,
        Spike_Trap = 777,
        Spike_Trap_Activate = 778,
        Bed_of_coals = 875,
        Churning_earth = 994
    };
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

    std::vector<Effect*> aoe_effects;
    std::unordered_map<uint32_t, EffectSettings*> aoe_effect_settings;
    std::unordered_map<uint32_t, EffectTrigger*> aoe_effect_triggers;

    GW::HookEntry StoC_Hook;

    unsigned int vertices_max = 0x1000; // max number of vertices to draw in one call
}
void EffectRenderer::LoadDefaults() {
    aoe_effect_settings.clear();
    aoe_effect_settings.emplace(Maelstrom, new EffectSettings("Maelstrom", Maelstrom, GW::Constants::Range::Adjacent, 10000));
    aoe_effect_settings.emplace(Chaos_storm, new EffectSettings("Chaos Storm", Chaos_storm, GW::Constants::Range::Adjacent, 10000));
    aoe_effect_settings.emplace(Savannah_heat, new EffectSettings("Savannah Heat", Savannah_heat, GW::Constants::Range::Adjacent, 5000));
    aoe_effect_settings.emplace(Breath_of_fire, new EffectSettings("Breath of Fire", Breath_of_fire, GW::Constants::Range::Adjacent, 5000));
    aoe_effect_settings.emplace(Maelstrom, new EffectSettings("Lava font", Lava_font, GW::Constants::Range::Adjacent, 5000));
    aoe_effect_settings.emplace(Churning_earth, new EffectSettings("Churning Earth", Churning_earth, GW::Constants::Range::Nearby, 5000));

    aoe_effect_settings.emplace(Barbed_Trap, new EffectSettings("Barbed Trap", Barbed_Trap, GW::Constants::Range::Adjacent, 90000, GW::Packet::StoC::GenericValue::STATIC_HEADER));
    aoe_effect_triggers.emplace(Barbed_Trap_Activate, new EffectTrigger(Barbed_Trap, 2000, GW::Constants::Range::Nearby));
    aoe_effect_settings.emplace(Flame_Trap, new EffectSettings("Flame Trap", Flame_Trap, GW::Constants::Range::Adjacent, 90000, GW::Packet::StoC::GenericValue::STATIC_HEADER));
    aoe_effect_triggers.emplace(Flame_Trap_Activate, new EffectTrigger(Flame_Trap, 2000, GW::Constants::Range::Nearby));
    aoe_effect_settings.emplace(Spike_Trap, new EffectSettings("Spike Trap", Barbed_Trap, GW::Constants::Range::Adjacent, 90000, GW::Packet::StoC::GenericValue::STATIC_HEADER));
    aoe_effect_triggers.emplace(Spike_Trap_Activate, new EffectTrigger(Spike_Trap, 2000, GW::Constants::Range::Nearby));
}
void EffectRenderer::Terminate() {
    VBuffer::Terminate();
    for (const auto& settings : aoe_effect_settings) {
        delete settings.second;
    }
    aoe_effect_settings.clear();
    for (const auto& triggers : aoe_effect_triggers) {
        delete triggers.second;
    }
    aoe_effect_triggers.clear();
}
void EffectRenderer::Invalidate() {
    VBuffer::Invalidate();
    for (const auto p : aoe_effects) {
        delete p;
    }
    aoe_effects.clear();
    for (const auto& trigger : aoe_effect_triggers) {
        trigger.second->triggers_handled.clear();
    }
}
void EffectRenderer::LoadSettings(ToolboxIni* ini, const char* section) {
    Invalidate();
    LoadDefaults();
    for (const auto& settings : aoe_effect_settings) {
        char color_buf[64];
        sprintf(color_buf, "color_aoe_effect_%d", settings.first);
        settings.second->color = Colors::Load(ini, section, color_buf, settings.second->color);
    }
}
void EffectRenderer::SaveSettings(ToolboxIni* ini, const char* section) const {
    for (const auto& settings : aoe_effect_settings) {
        char color_buf[64];
        sprintf(color_buf, "color_aoe_effect_%d", settings.first);
        Colors::Save(ini, section, color_buf, settings.second->color);
    }
}
void EffectRenderer::DrawSettings() {
    bool confirm = false;
    if (ImGui::SmallConfirmButton("Restore Defaults", &confirm)) {
        LoadDefaults();
    }
    for (const auto& s : aoe_effect_settings) {
        ImGui::PushID(static_cast<int>(s.first));
        Colors::DrawSettingHueWheel("", &s.second->color, 0);
        ImGui::SameLine();
        ImGui::Text(s.second->name.c_str());
        ImGui::PopID();
    }

}
void EffectRenderer::RemoveTriggeredEffect(uint32_t effect_id, GW::Vec2f* pos) {
    auto it1 = aoe_effect_triggers.find(effect_id);
    if (it1 == aoe_effect_triggers.end())
        return;
    auto trigger = it1->second;
    auto settings = aoe_effect_settings.find(trigger->triggered_effect_id)->second;
    std::pair<float, float> posp = { pos->x,pos->y };
    auto trap_handled = trigger->triggers_handled.find(posp);
    if (trap_handled != trigger->triggers_handled.end() && TIMER_DIFF(trap_handled->second) < 5000) {
        return; // Already handled this trap, e.g. Spike Trap triggers 3 times over 2 seconds; we only care about the first.
    }
    trigger->triggers_handled.emplace(posp, TIMER_INIT());
    std::lock_guard<std::recursive_mutex> lock(effects_mutex);
    Effect* closest = nullptr;
    float closestDistance = GW::Constants::SqrRange::Nearby;
    uint32_t closest_idx = 0;
    for (size_t i = 0; i < aoe_effects.size();i++) {
        Effect* effect = aoe_effects[i];
        if (!effect || effect->effect_id != settings->effect_id)
            continue;
        // Need to estimate position; player may have moved on cast slightly.
        float newDistance = GW::GetSquareDistance(*pos,effect->pos);
        if (newDistance > closestDistance)
            continue;
        closest_idx = i;
        closest = effect;
        closestDistance = newDistance;
    }
    if (closest) {
        // Trigger this trap to time out in 2 seconds' time. Increase damage radius from adjacent to nearby.
        closest->start = TIMER_INIT();
        closest->duration = trigger->duration;
        closest->circle.range = trigger->range;
    }
}

void EffectRenderer::PacketCallback(GW::Packet::StoC::GenericValue* pak) {
    if (!initialized) return;
    if (pak->value_id != 21) // Effect on agent
        return;
    auto it = aoe_effect_settings.find(pak->value);
    if (it == aoe_effect_settings.end())
        return;
    const auto settings = it->second;
    if (settings->stoc_header && settings->stoc_header != pak->header)
        return;
    const auto* caster = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(pak->agent_id));
    if (!caster || caster->allegiance != GW::Constants::Allegiance::Enemy) return;
    aoe_effects.push_back(new Effect(pak->value, caster->pos.x, caster->pos.y, settings->duration, settings->range, &settings->color));
}
void EffectRenderer::PacketCallback(GW::Packet::StoC::GenericValueTarget* pak) {
    if (!initialized) return;
    if (pak->Value_id != 20) // Effect on target
        return;
    auto it = aoe_effect_settings.find(pak->value);
    if (it == aoe_effect_settings.end())
        return;
    auto settings = it->second;
    if (settings->stoc_header && settings->stoc_header != pak->header)
        return;
    if (pak->caster == pak->target) return;
    GW::AgentLiving* caster = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(pak->caster));
    if (!caster || caster->allegiance != GW::Constants::Allegiance::Enemy) return;
    GW::Agent* target = GW::Agents::GetAgentByID(pak->target);
    if (!target) return;
    aoe_effects.push_back(new Effect(pak->value, target->pos.x, target->pos.y, settings->duration, settings->range, &settings->color));
}
void EffectRenderer::PacketCallback(GW::Packet::StoC::PlayEffect* pak) {
    if (!initialized) return;
    // TODO: Fire storm and Meteor shower have no caster!
    // Need to record GenericValueTarget with value_id matching these skills, then roughly match the coords after.
    RemoveTriggeredEffect(pak->effect_id, &pak->coords);
    auto it = aoe_effect_settings.find(pak->effect_id);
    if (it == aoe_effect_settings.end())
        return;
    auto settings = it->second;
    if (settings->stoc_header && settings->stoc_header != pak->header)
        return;
    GW::AgentLiving* a = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(pak->agent_id));
    if (!a || a->allegiance != GW::Constants::Allegiance::Enemy) return;
    aoe_effects.push_back(new Effect(pak->effect_id, pak->coords.x, pak->coords.y, settings->duration, settings->range, &settings->color));
}
void EffectRenderer::Initialize(IDirect3DDevice9* device) {
    if (initialized)
        return;
    initialized = true;
    type = D3DPT_LINELIST;

    HRESULT hr = device->CreateVertexBuffer(sizeof(D3DVertex) * vertices_max, 0,
        D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
    if (FAILED(hr)) {
        printf("Error setting up PingsLinesRenderer vertex buffer: HRESULT: 0x%lX\n", hr);
    }
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(&StoC_Hook, [&](GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer*) {
        need_to_clear_effects = true;
        });
}
void EffectRenderer::Render(IDirect3DDevice9* device) {
    Initialize(device);
    DrawAoeEffects(device);
}
void EffectRenderer::DrawAoeEffects(IDirect3DDevice9* device) {
    if (need_to_clear_effects) {
        Invalidate();
        need_to_clear_effects = false;
    }
    if (aoe_effects.empty())
        return;
    std::lock_guard<std::recursive_mutex> lock(effects_mutex);
    size_t effect_size = aoe_effects.size();
    for (size_t i = 0; i < effect_size; i++) {
        Effect* effect = aoe_effects[i];
        if (!effect)
            continue;
        if (TIMER_DIFF(effect->start) > static_cast<clock_t>(effect->duration)) {
            aoe_effects.erase(aoe_effects.begin() + static_cast<int>(i));
            delete effect;
            i--;
            effect_size--;
            continue;
        }
        const auto translate = DirectX::XMMatrixTranslation(effect->pos.x, effect->pos.y, 0.0f);
        const auto scale = DirectX::XMMatrixScaling(effect->circle.range, effect->circle.range, 1.0f);
        auto world = scale * translate;
        device->SetTransform(D3DTS_WORLD, reinterpret_cast<D3DMATRIX*>(&world));
        effect->circle.Render(device);
    }
}

void EffectCircle::Initialize(IDirect3DDevice9* device) {
    type = D3DPT_LINESTRIP;
    count = 16; // polycount
    const auto vertex_count = count + 1;
    D3DVertex* vertices = nullptr;

    if (buffer) buffer->Release();
    device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, 0,
        D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
    buffer->Lock(0, sizeof(D3DVertex) * vertex_count,
        (VOID**)&vertices, D3DLOCK_DISCARD);

    for (size_t i = 0; i < count; ++i) {
        float angle = i * (DirectX::XM_2PI / count);
        vertices[i].x = std::cos(angle);
        vertices[i].y = std::sin(angle);
        vertices[i].z = 0.0f;
        vertices[i].color = *color;
    }
    vertices[count] = vertices[0];

    buffer->Unlock();
}
