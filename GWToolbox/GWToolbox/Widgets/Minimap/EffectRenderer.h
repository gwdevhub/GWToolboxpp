#pragma once

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Packets/StoC.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/GameContainers/GamePos.h>

#include "VBuffer.h"
#include "Timer.h"

#include "Defines.h"

#include <mutex>

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
		Effect(uint32_t _effect_id, float _x, float _y, int _duration = 10000, float range = GW::Constants::Range::Adjacent) : effect_id(_effect_id), pos(_x,_y), duration(_duration), start(TIMER_INIT()) {
			circle.range = range;
		};
		clock_t start;
		const uint32_t effect_id;
		const GW::Vec2f pos;
		int duration;
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

	void RemoveTriggeredEffect(uint32_t effect_id, GW::Vec2f* pos);
	void RemoveEffect(Effect* effect);
	void DrawAoeEffects(IDirect3DDevice9* device);
	std::vector<Effect*> aoe_effects;
	bool need_to_clear_effects = false;

	std::recursive_mutex effects_mutex;

	GW::HookEntry StoC_Hook;
	
	Color aoe_color = 0xFFFF0000;
	unsigned int vertices_max = 0x1000;	// max number of vertices to draw in one call
};
