#pragma once

#include <Windows.h>
#include <d3d9.h>

#include "VBuffer.h"

class PmapRenderer : public VBuffer {
public:
	PmapRenderer() : VBuffer() {}

protected:
	void Initialize(IDirect3DDevice9* device) override;

private:
};
