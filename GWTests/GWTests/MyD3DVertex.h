#pragma once

#include <Windows.h>

struct Vertex {
	float x;
	float y;
	float z;
	DWORD color;
};
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_DIFFUSE)
