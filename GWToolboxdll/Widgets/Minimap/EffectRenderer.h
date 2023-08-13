#pragma once

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Packets/StoC.h>

#include <Widgets/Minimap/VBuffer.h>

class EffectRenderer : public VBuffer {
    friend class Minimap;

public:
    void Render(IDirect3DDevice9* device) override;

    void Invalidate() override;
    void Terminate() override;
    void PacketCallback(const GW::Packet::StoC::GenericValue* pak) const;
    void PacketCallback(const GW::Packet::StoC::GenericValueTarget* pak) const;
    void PacketCallback(GW::Packet::StoC::PlayEffect* pak) const;

    static void LoadDefaults();
    static void DrawSettings();
    void LoadSettings(const ToolboxIni* ini, const char* section);
    static void SaveSettings(ToolboxIni* ini, const char* section);

private:
    void Initialize(IDirect3DDevice9* device) override;

    static void RemoveTriggeredEffect(uint32_t effect_id, GW::Vec2f* pos);
    void DrawAoeEffects(IDirect3DDevice9* device);
};
