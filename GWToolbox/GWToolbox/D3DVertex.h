#pragma once

#include <Windows.h>

struct D3DVertex {
	float x;
	float y;
	float z;
	DWORD color;
};
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_DIFFUSE)
