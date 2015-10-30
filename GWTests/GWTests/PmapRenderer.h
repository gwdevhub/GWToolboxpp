#pragma once

#include <Windows.h>
#include <d3d9.h>

#include "MyD3DVertex.h"
#include "Renderer.h"

class PmapRenderer : public Renderer {
public:
	PmapRenderer() : Renderer() {}

protected:
	void Initialize(IDirect3DDevice9* device) override;

private:
};
