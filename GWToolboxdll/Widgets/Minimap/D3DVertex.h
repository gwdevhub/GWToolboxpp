#pragma once

struct D3DVertex {
    float x;
    float y;
    float z;
    D3DCOLOR color;
};
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_DIFFUSE)
