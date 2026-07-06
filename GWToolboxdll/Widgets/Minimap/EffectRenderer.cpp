#include "stdafx.h"

#include <Color.h>
#include <Utils/AoeEffects.h>
#include <Widgets/Minimap/EffectRenderer.h>

namespace {
    class EffectCircle : public D3DVertexBuffer {
        void Initialize(IDirect3DDevice9* device) override;

    public:
        ~EffectCircle() override { D3DVertexBuffer::Terminate(); }
        Color color = 0xFFFF0000;
    };

    void EnsureColor(EffectCircle& circle, const Color color)
    {
        if (circle.color == color) return;
        circle.color = color;
        circle.Invalidate(); // rebake the vertices with the new colour on next render
    }

    std::unordered_map<uint64_t, std::unique_ptr<EffectCircle>> circles;
    std::vector<AoeEffects::ActiveEffect> effects_snapshot;
}

void EffectRenderer::Terminate()
{
    D3DVertexBuffer::Terminate();
    circles.clear();
}

void EffectRenderer::Invalidate()
{
    D3DVertexBuffer::Invalidate();
    circles.clear();
}

void EffectRenderer::LoadSettings(const SettingsDoc& doc, const ToolboxIni* legacy, const char* section)
{
    Invalidate();
    AoeEffects::LoadDefaults();
    for (const auto& settings : AoeEffects::GetEffectSettings()) {
        char color_buf[64];
        sprintf(color_buf, "color_aoe_effect_%d", settings.first);
        Colors::SettingColor staged(settings.second->color);
        if (doc.Get(section, color_buf, staged)) {
            settings.second->color = staged.value;
        }
        else if (legacy) {
            settings.second->color = Colors::Load(legacy, section, color_buf, settings.second->color);
        }
    }
}

void EffectRenderer::SaveSettings(SettingsDoc& doc, const char* section)
{
    for (const auto& settings : AoeEffects::GetEffectSettings()) {
        char color_buf[64];
        sprintf(color_buf, "color_aoe_effect_%d", settings.first);
        doc.Set(section, color_buf, Colors::SettingColor(settings.second->color));
    }
}

void EffectRenderer::DrawSettings()
{
    AoeEffects::DrawColorSettings();
}

void EffectRenderer::Initialize(IDirect3DDevice9*)
{
    if (initialized) {
        return;
    }
    initialized = true;
    AoeEffects::Initialize();
}

void EffectRenderer::Render(IDirect3DDevice9* device)
{
    Initialize(device);
    DrawAoeEffects(device);
}

void EffectRenderer::DrawAoeEffects(IDirect3DDevice9* device)
{
    AoeEffects::GetActiveEffects(effects_snapshot);
    if (effects_snapshot.empty()) {
        circles.clear();
        return;
    }
    std::erase_if(circles, [](const auto& entry) {
        return std::ranges::none_of(effects_snapshot, [&entry](const auto& e) { return e.uid == entry.first; });
    });
    for (const auto& effect : effects_snapshot) {
        auto& circle = circles[effect.uid];
        if (!circle) {
            circle = std::make_unique<EffectCircle>();
        }
        EnsureColor(*circle, effect.color);
        const auto translate = DirectX::XMMatrixTranslation(effect.pos.x, effect.pos.y, 0.0f);
        const auto scale = DirectX::XMMatrixScaling(effect.range, effect.range, 1.0f);
        auto world = scale * translate;
        device->SetTransform(D3DTS_WORLD, reinterpret_cast<D3DMATRIX*>(&world));
        circle->Render(device);
    }
}

void EffectCircle::Initialize(IDirect3DDevice9* device)
{
    constexpr size_t poly_count = 16;
    type = D3DPT_LINESTRIP;

    vertices.resize(poly_count + 1);
    for (size_t i = 0; i < poly_count; i++) {
        const float angle = i * (DirectX::XM_2PI / poly_count);
        vertices[i] = {std::cos(angle), std::sin(angle), 0.f, color};
    }
    vertices[poly_count] = vertices[0];

    D3DVertexBuffer::Initialize(device);

    count = poly_count;
}
