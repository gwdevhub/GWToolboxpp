#pragma once

#include <deque>

#include "VBuffer.h"
#include "Timer.h"

class PingsLinesRenderer {
	struct CtoS_P37 {
		const DWORD header = 0x25;
		DWORD session_id = 0;
		DWORD NumberPts;
		struct {
			short x;
			short y;
		} points[8];
		DWORD unk[8];
	};
	struct Ping {
		Ping() : start(TBTimer::init()) {}
		clock_t start;
		virtual float GetX() = 0;
		virtual float GetY() = 0;
		virtual float GetScale() { return 1.0f; }
	};
	struct TerrainPing : Ping {
		TerrainPing(short _x, short _y) : Ping(), x(_x), y(_y) {}
		short x;
		short y;
		float GetX() override { return x * 100.0f; }
		float GetY() override { return y * 100.0f; }
		float GetScale() override { return 2.0f; }
	};
	struct AgentPing : Ping {
		AgentPing(DWORD _id) : Ping(), id(_id) {}
		DWORD id;
		float GetX() override;
		float GetY() override;
	};
	class PingCircle : public VBuffer {
		D3DVertex* vertices;
		unsigned int vertex_count;
		void Initialize(IDirect3DDevice9* device) override;
	};

public:
	PingsLinesRenderer();

	void Render(IDirect3DDevice9* device);
	inline void Invalidate() { 
		ping_circle.Invalidate();
		for (Ping* p : pings) delete p;
		pings.clear();
	}

	void CreatePing(float x, float y);

private:
	PingCircle ping_circle;
	std::deque<Ping*> pings;
};

