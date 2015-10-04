/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_DIRECT3D8X_HPP
#define OSHGUI_DRAWING_DIRECT3D8X_HPP

#include <d3dx8.h>

/**
 * Rebuild einiger D3DX8 Funktionen, da ein Linken gegen die D3DX8 Bibliothek nicht konfliktfrei ist.
 */

D3DXMATRIX* MatrixMultiply(D3DXMATRIX *pout, const D3DXMATRIX *pm1, const D3DXMATRIX *pm2);
D3DXMATRIX* MatrixIdentity(D3DXMATRIX *pOut);
D3DXMATRIX* MatrixOrthoOffCenterRH(D3DXMATRIX *pout, float l, float r, float b, float t, float zn, float zf);
D3DXMATRIX* MatrixLookAtRH(D3DXMATRIX *out, const D3DXVECTOR3 *eye, const D3DXVECTOR3 *at, const D3DXVECTOR3 *up);
D3DXMATRIX* MatrixPerspectiveFovRH(D3DXMATRIX *pout, float fovy, float aspect, float zn, float zf);
D3DXMATRIX* MatrixTransformation(D3DXMATRIX *pout, const D3DXVECTOR3 *pscalingcenter, const D3DXQUATERNION *pscalingrotation, const D3DXVECTOR3 *pscaling, const D3DXVECTOR3 *protationcenter, const D3DXQUATERNION *protation, const D3DXVECTOR3 *ptranslation);

HRESULT LoadSurfaceFromMemory(IDirect3DSurface8 *destSurface, const PALETTEENTRY *destPaletteEntry, const RECT *destRect, const void *srcData, D3DFORMAT format, UINT srcPitch, const PALETTEENTRY *srcPaletteEntry, const RECT *srcRect, DWORD filter, D3DCOLOR color);

#endif