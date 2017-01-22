#pragma once

#include <deque>
#include <vector>
#include <map>

#include "VBuffer.h"
#include "Timer.h"

class PingsLinesRenderer : public VBuffer {
	const float drawing_scale = 96.0f;

	struct ShortPos {
		ShortPos() : x(0), y(0) {}
		ShortPos(short _x, short _y) : x(_x), y(_y) {}
		short x;
		short y;
	};
	struct DrawingLine {
		DrawingLine() : start(TBTimer::init()) {}
		clock_t start;
		float x1, y1;
		float x2, y2;
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
		TerrainPing(float _x, float _y) : Ping(), x(_x), y(_y) {}
		float x, y;
		float GetX() override { return x; }
		float GetY() override { return y; }
		float GetScale() override { return 2.0f; }
	};
	struct AgentPing : Ping {
		AgentPing(DWORD _id) : Ping(), id(_id) {}
		DWORD id;
		float GetX() override;
		float GetY() override;
		float GetScale() override;
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

	void SetVisible(bool v) { visible_ = v; }

	bool OnMouseDown(float x, float y);
	bool OnMouseMove(float x, float y);
	bool OnMouseUp();

private:
	void Initialize(IDirect3DDevice9* device) override;

	inline short ToShortPos(float n) {
		return static_cast<short>(std::lroundf(n / drawing_scale));
	}
	inline void BumpSessionID() { if (--session_id < 0) session_id = 7; }
	void SendQueue();

	PingCircle ping_circle;
	std::deque<Ping*> pings;

	std::map<DWORD, PlayerDrawing> drawings;

	bool visible_;

	// for pings and drawings
	const long show_interval = 10;
	const long queue_interval = 25;
	const long send_interval = 250;
	bool mouse_down;
	bool mouse_moved; // true if moved since last pressed
	float mouse_x, mouse_y;
	int session_id;
	clock_t lastshown;
	clock_t lastsent;
	clock_t lastqueued;
	std::vector<ShortPos> queue;

	DWORD color_drawings;
	DWORD color_pings;

	// for the gpu
	D3DVertex* vertices;		// vertices array
	unsigned int vertices_count;// count of vertices
	unsigned int vertices_max;	// max number of vertices to draw in one call
};
