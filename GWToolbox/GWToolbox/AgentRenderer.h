#pragma once

#include <GWCA\GWStructures.h>
#include <GWCA\AgentMgr.h>

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
		Color(int _a, int _r, int _g, int _b) 
			: a(_a), r(_r), g(_g), b(_b) {}
		Color(int _r, int _g, int _b) : Color(255, _r, _g, _b) {}
		const Color operator + (const Color& c) const {
			return Color(a + c.a, r + c.r, g + c.g, b + c.b);
		}
		const Color operator + (const int c) const {
			return Color(r + c, g + c, b + c);
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

	enum Shape_e { Tear, Circle, Quad };
	struct Shape_t {
		std::vector<GWCA::Vector2f> vertices;
		std::vector<int> colors;
		void AddVertex(float x, float y, int color);
	};

	void Initialize(IDirect3DDevice9* device) override;

	void Enqueue(GWCA::GW::Agent* agent);
	Color GetColor(GWCA::GW::Agent* agent) const;
	float GetSize(GWCA::GW::Agent* agent) const;
	Shape_e GetShape(GWCA::GW::Agent* agent) const;

	void Enqueue(Shape_e shape, GWCA::GW::Agent* agent, float size, Color color);

	void CheckFlush(IDirect3DDevice9* device);
	void Flush(IDirect3DDevice9* device);

	D3DVertex* vertices;		// vertices array
	unsigned int triangle_count;// count of triangles
	unsigned int vertices_max;	// max number of vertices to draw in one call
	unsigned int triangles_max; // max number of triangles to draw in one call
	unsigned int max_shape_tris;// max number of triangles in a single shape

	Shape_t shapes[3];
};
