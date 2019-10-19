#pragma once

#include "Color.h"

#include "VBuffer.h"


class RangeRenderer : public VBuffer {
private:
	static const int num_circles = 5;
	static const int circle_points = 64;
	static const int circle_vertices = 65;

public:
	void Render(IDirect3DDevice9* device) override;
	void SetDrawCenter(bool b) { draw_center_ = b; }

	void DrawSettings();
	void LoadSettings(CSimpleIni* ini, const char* section);
	void SaveSettings(CSimpleIni* ini, const char* section) const;

private:
	void CreateCircle(D3DVertex* vertices, float radius, DWORD color);
	void Initialize(IDirect3DDevice9* device) override;

	bool HaveHos();

	bool checkforhos_ = true;
	bool havehos_ = false;

	bool draw_center_ = false;

	Color color_range_hos = 0;
	Color color_range_aggro = 0;
	Color color_range_cast = 0;
	Color color_range_spirit = 0;
	Color color_range_compass = 0;
};
