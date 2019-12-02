#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/GameContainers/GamePos.h>

#include "VBuffer.h"
#include "Timer.h"

#include "Defines.h"

#include "Color.h"

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
		Effect(uint32_t _effect_id, float _x, float _y) : Effect(_effect_id,_x,_y,duration)  {};
		Effect(uint32_t _effect_id, float _x, float _y, uint32_t _duration) : Effect(_effect_id, _x, _y, duration, GW::Constants::Range::Adjacent) {};
		Effect(uint32_t _effect_id, float _x, float _y, uint32_t _duration, float range) : effect_id(_effect_id), y(_y), x(_x), duration(_duration), start(TIMER_INIT()) {
			circle.range = range;
		};
		clock_t start;
		const uint32_t effect_id;
		const float x, y;
		int duration = 10000;
		EffectCircle circle;
	};

public:

	void Render(IDirect3DDevice9* device) override;

	inline void Invalidate() {
		for (Effect* p : aoe_effects) delete p;
		aoe_effects.clear();
	}
	void PacketCallback(GW::Packet::StoC::GenericValue* pak);
	void PacketCallback(GW::Packet::StoC::GenericValueTarget* pak);
	void PacketCallback(GW::Packet::StoC::PlayEffect* pak);

	void DrawSettings();
	void LoadSettings(CSimpleIni* ini, const char* section);
	void SaveSettings(CSimpleIni* ini, const char* section) const;

private:
	void Initialize(IDirect3DDevice9* device) override;

	void DrawAoeEffects(IDirect3DDevice9* device);
	std::deque<Effect*> aoe_effects;
	
	Color aoe_color = 0xFFFF0000;
	unsigned int vertices_max = 0x1000;	// max number of vertices to draw in one call
};
