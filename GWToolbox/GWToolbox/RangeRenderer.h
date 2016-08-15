#pragma once

#include "VBuffer.h"

class RangeRenderer : public VBuffer {
private:
	static const int num_circles = 5;
	static const int circle_points = 64;
	static const int circle_vertices = 65;
public:
	void Render(IDirect3DDevice9* device) override;
	//void CheckAgainForHos() { checkforhos_ = true; }

private:
	void CreateCircle(D3DVertex* vertices, float radius, DWORD color);
	void Initialize(IDirect3DDevice9* device) override;

	bool HaveHos();

	bool checkforhos_;
	bool havehos_;
};
