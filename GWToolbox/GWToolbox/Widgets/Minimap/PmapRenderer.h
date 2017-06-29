#pragma once

#include <Windows.h>
#include <d3d9.h>

#include "VBuffer.h"
#include "Color.h"

class PmapRenderer : public VBuffer {
public:
	// Triangle 1: (XTL, YT) (XTR, YT), (XBL, YB)
	// Triangle 2: (XBL, YB), (XTR, YT), (XBR, YB)
	void Render(IDirect3DDevice9* device) override;

	void DrawSettings();
	void LoadSettings(CSimpleIni* ini, const char* section);
	void SaveSettings(CSimpleIni* ini, const char* section) const;

protected:
	void Initialize(IDirect3DDevice9* device) override;

private:
	Color color_map;
	Color color_mapshadow;

	size_t trapez_count_;
	size_t tri_count_; // of just 1 batch (map)
	size_t total_tri_count_; // including shadow
	size_t vert_count_;
	size_t total_vert_count_; // including shadow
};
