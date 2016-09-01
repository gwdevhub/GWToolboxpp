#pragma once

#include <vector>

#include <GWCA\GameEntities\Agent.h>

#include "VBuffer.h"

class AgentRenderer : public VBuffer {
public:
	AgentRenderer();

	void Render(IDirect3DDevice9* device) override;

private:
	struct Color {
		int a;
		int r;
		int g;
		int b;
		Color() : a(0), r(0), g(0), b(0) {}
		Color(DWORD c) : 
			a((c >> 24) & 0xFF),
			r((c >> 16) & 0xFF),
			g((c >> 8) & 0xFF),
			b((c) & 0xFF)
		{}
		Color(int _a, int _r, int _g, int _b) 
			: a(_a), r(_r), g(_g), b(_b) {}
		Color(int _r, int _g, int _b) : Color(255, _r, _g, _b) {}
		const Color operator + (const Color& c) const {
			return Color(a + c.a, r + c.r, g + c.g, b + c.b);
		}
		void Clamp() {
			if (r < 0) r = 0;
			if (g < 0) g = 0;
			if (b < 0) b = 0;
			if (r > 255) r = 255;
			if (g > 255) g = 255;
			if (b > 255) b = 255;
		}
		DWORD GetDXColor() {
			return D3DCOLOR_ARGB(a, r, g, b);
		}
	};

	enum Shape_e { Tear, Circle, Quad, BigCircle };
	struct Shape_t {
		std::vector<GW::Vector2f> vertices;
		std::vector<Color> colors;
		void AddVertex(float x, float y, Color color);
	};
	static const size_t shape_size = 4;
	Shape_t shapes[shape_size];

	void Initialize(IDirect3DDevice9* device) override;

	void Enqueue(GW::Agent* agent);
	Color GetColor(GW::Agent* agent) const;
	float GetSize(GW::Agent* agent) const;
	Shape_e GetShape(GW::Agent* agent) const;

	void Enqueue(Shape_e shape, GW::Agent* agent, float size, Color color);

	D3DVertex* vertices;		// vertices array
	unsigned int vertices_count;// count of vertices
	unsigned int vertices_max;	// max number of vertices to draw in one call
	unsigned int max_shape_verts;// max number of triangles in a single shape

	Color color_eoe;
	Color color_target;
	Color color_player;
	Color color_player_dead;
	Color color_signpost;
	Color color_item;
	Color color_hostile;
	Color color_hostile_damaged;
	Color color_hostile_dead;
	Color color_neutral;
	Color color_ally_party;
	Color color_ally_npc;
	Color color_ally_spirit;
	Color color_ally_minion;
	Color color_ally_dead;
};
