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



public:

    void Render(IDirect3DDevice9* device) override;

    void Invalidate() override;
    void Terminate() override;
    void PacketCallback(GW::Packet::StoC::GenericValue* pak);
    void PacketCallback(GW::Packet::StoC::GenericValueTarget* pak);
    void PacketCallback(GW::Packet::StoC::PlayEffect* pak);

    void LoadDefaults();
    void DrawSettings();
    void LoadSettings(ToolboxIni* ini, const char* section);
    void SaveSettings(ToolboxIni* ini, const char* section) const;

private:
    void Initialize(IDirect3DDevice9* device) override;

    void RemoveTriggeredEffect(uint32_t effect_id, GW::Vec2f* pos);
    void DrawAoeEffects(IDirect3DDevice9* device);
};
