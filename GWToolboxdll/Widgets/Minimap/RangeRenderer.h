#pragma once

#include "Color.h"

#include "VBuffer.h"


class RangeRenderer : public VBuffer {
	class TargetRange : public VBuffer {
		void Initialize(IDirect3DDevice9* device) override;
	public:
		Color color = 0;
	};
private:
	static const size_t num_circles = 6;
	static const size_t circle_points = 64;
	static const size_t circle_vertices = 65;
	unsigned int vertex_count;
	D3DVertex* vertices;

public:
	void Render(IDirect3DDevice9* device) override;
	void SetDrawCenter(bool b) { draw_center_ = b; }

	void DrawSettings();
	void LoadSettings(CSimpleIni* ini, const char* section);
	void SaveSettings(CSimpleIni* ini, const char* section) const;

private:
	void CreateCircle(D3DVertex* vertices, float radius, DWORD color);
	void DrawTargetRange(IDirect3DDevice9* device);
	void Initialize(IDirect3DDevice9* device) override;

	bool HaveHos();

	bool checkforhos_ = true;
	bool havehos_ = false;

	bool draw_center_ = false;

	TargetRange chainAggroRange;
	TargetRange rezzAggroRange;

	Color color_range_hos = 0;
	Color color_range_aggro = 0;
	Color color_range_cast = 0;
	Color color_range_spirit = 0;
	Color color_range_compass = 0;
};
