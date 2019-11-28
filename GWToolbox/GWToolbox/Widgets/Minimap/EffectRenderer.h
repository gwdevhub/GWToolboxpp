#pragma once

#include <GWCA/Packets/StoC.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/GameContainers/GamePos.h>

#include "VBuffer.h"
#include "Timer.h"

#include "Color.h"

class EffectRenderer : public VBuffer {
	friend class Minimap;
	const float drawing_scale = 96.0f;
	class EffectCircle : public VBuffer {
		void Initialize(IDirect3DDevice9* device) override;
	public:
		Color color = 0;
	};
	struct Effect {
		Effect(uint32_t _effect_id, float _x, float _y) : effect_id(_effect_id), y(_y), x(_x), start(TIMER_INIT()) {
			circle.color = Colors::ARGB(128, 255, 0, 0);
			if ((circle.color & IM_COL32_A_MASK) == 0) circle.color |= Colors::ARGB(128, 0, 0, 0);
		};
		clock_t start;
		const uint32_t effect_id;
		const float x, y;
		int duration = 10000;
		EffectCircle circle;
	};

public:
	EffectRenderer();

	void Render(IDirect3DDevice9* device) override;

	inline void Invalidate() {
		for (Effect* p : pings) delete p;
		pings.clear();
	}
	void PacketCallback(GW::Packet::StoC::GenericValue* pak);
	void PacketCallback(GW::Packet::StoC::GenericValueTarget* pak);
	void PacketCallback(GW::Packet::StoC::PlayEffect* pak);

	void DrawSettings();
	void LoadSettings(CSimpleIni* ini, const char* section);
	void SaveSettings(CSimpleIni* ini, const char* section) const;

private:
	void Initialize(IDirect3DDevice9* device) override;

	void DrawPings(IDirect3DDevice9* device);

	inline short ToShortPos(float n) {
		return static_cast<short>(std::lroundf(n / drawing_scale));
	}
	inline void BumpSessionID() { if (--session_id < 0) session_id = 7; }

	std::deque<Effect*> pings;

	// for pings and drawings
	const long show_interval = 10;
	const long queue_interval = 25;
	const long send_interval = 250;
	bool mouse_down = false;
	bool mouse_moved = false; // true if moved since last pressed
	float mouse_x = 0.f;
	float mouse_y = 0.f;
	int session_id = 0;
	clock_t lastshown = 0;
	clock_t lastsent = 0;
	clock_t lastqueued = 0;
	std::vector<GW::UI::CompassPoint> queue;

	Color color_drawings = 0;
	Color color_shadowstep_line = 0;
	Color color_shadowstep_line_maxrange = 0;
	float maxrange_interp_begin = 0.85f;
	float maxrange_interp_end = 0.95f;
	bool reduce_ping_spam = false;

	// for markers
	GW::Vec2f shadowstep_location;
	DWORD recall_target = 0;

	// for the gpu
	D3DVertex* vertices = nullptr;	// vertices array
	unsigned int vertices_count = 0;// count of vertices
	unsigned int vertices_max = 0;	// max number of vertices to draw in one call
};
