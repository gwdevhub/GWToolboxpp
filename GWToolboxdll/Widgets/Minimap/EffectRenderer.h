#pragma once

#include <D3DContainers.h>

class SettingsDoc;

// Minimap view of the shared AoE effect store (Utils/AoeEffects): draws each active effect as a
// circle. Also owns the persistence of the shared per-skill colours (kept in the minimap settings
// section for backwards compatibility).
class EffectRenderer : public D3DVertexBuffer {
    friend class Minimap;

public:
    void Render(IDirect3DDevice9* device) override;

    void Invalidate() override;
    void Terminate() override;

    static void DrawSettings();
    void LoadSettings(const SettingsDoc& doc, const ToolboxIni* legacy, const char* section);
    static void SaveSettings(SettingsDoc& doc, const char* section);

private:
    void Initialize(IDirect3DDevice9* device) override;

    void DrawAoeEffects(IDirect3DDevice9* device);
};
