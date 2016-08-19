#pragma once

#include <deque>
#include <map>

#include "VBuffer.h"
#include "Timer.h"

class PingsLinesRenderer : public VBuffer {
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
	struct DrawingLine {
		DrawingLine() : start(TBTimer::init()) {}
		clock_t start;
		short x1, y1;
		short x2, y2;
	};
	struct PlayerDrawing {
		PlayerDrawing() : player(0), session(0) {}
		DWORD player;
		DWORD session;
		std::deque<DrawingLine> lines;
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

	void Render(IDirect3DDevice9* device) override;

	inline void Invalidate() {
		ping_circle.Invalidate();
		for (Ping* p : pings) delete p;
		pings.clear();
	}

	void CreatePing(float x, float y);

	void SetVisible(bool v) { visible_ = v; }

private:
	void Initialize(IDirect3DDevice9* device) override;

	PingCircle ping_circle;
	std::deque<Ping*> pings;

	std::map<DWORD, PlayerDrawing> drawings;

	bool visible_;

	D3DVertex* vertices;		// vertices array
	unsigned int vertices_count;// count of vertices
	unsigned int vertices_max;	// max number of vertices to draw in one call
};

