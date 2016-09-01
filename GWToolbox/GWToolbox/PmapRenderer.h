#pragma once

#include <Windows.h>
#include <d3d9.h>

#include "VBuffer.h"

class PmapRenderer : public VBuffer {
public:
	PmapRenderer();

	// Triangle 1: (XTL, YT) (XTR, YT), (XBL, YB)
	// Triangle 2: (XBL, YB), (XTR, YT), (XBR, YB)
	void Render(IDirect3DDevice9* device) override;

protected:
	void Initialize(IDirect3DDevice9* device) override;

private:
	DWORD color_map;
	DWORD color_mapshadow;
	bool shadow_show;

	size_t trapez_count_;
	size_t tri_count_; // of just 1 batch (map)
	size_t total_tri_count_; // including shadow
	size_t vert_count_;
	size_t total_vert_count_; // including shadow
};
