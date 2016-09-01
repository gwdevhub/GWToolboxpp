#pragma once

#include "VBuffer.h"

class RangeRenderer : public VBuffer {
private:
	static const int num_circles = 5;
	static const int circle_points = 64;
	static const int circle_vertices = 65;
public:
	RangeRenderer();

	void Render(IDirect3DDevice9* device) override;

private:
	void CreateCircle(D3DVertex* vertices, float radius, DWORD color);
	void Initialize(IDirect3DDevice9* device) override;

	bool HaveHos();

	bool checkforhos_;
	bool havehos_;

	DWORD color_range_hos;
	DWORD color_range_aggro;
	DWORD color_range_cast;
	DWORD color_range_spirit;
	DWORD color_range_compass;
};
