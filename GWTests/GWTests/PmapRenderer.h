#pragma once

#include <Windows.h>
#include <d3d9.h>

#include "PathingMap.h"
#include "MyD3DVertex.h"
#include "Renderer.h"

class PmapRenderer : public Renderer {
public:
	PmapRenderer();

	void LoadMap(unsigned long file_hash);

protected:
	void Initialize(IDirect3DDevice9* device) override;

private:
	PathingMap pmap;
};
