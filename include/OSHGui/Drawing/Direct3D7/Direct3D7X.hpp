/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_DIRECT3D7X_HPP
#define OSHGUI_DRAWING_DIRECT3D7X_HPP

#define D3D_OVERLOADS
#include <d3dx.h>

/**
 * Rebuild einiger D3DX7 Funktionen.
 */

D3DMATRIX* MatrixMultiply(D3DMATRIX *pout, const D3DMATRIX *pm1, const D3DMATRIX *pm2);
D3DMATRIX* MatrixIdentity(D3DMATRIX *pOut);
D3DMATRIX* MatrixOrthoOffCenterRH(D3DMATRIX *pout, float l, float r, float b, float t, float zn, float zf);
D3DMATRIX* MatrixLookAtRH(D3DMATRIX *out, const D3DXVECTOR3 *eye, const D3DXVECTOR3 *at, const D3DXVECTOR3 *up);
D3DMATRIX* MatrixPerspectiveFovRH(D3DMATRIX *pout, float fovy, float aspect, float zn, float zf);
D3DMATRIX* MatrixTransformation(D3DMATRIX *pout, const D3DXVECTOR3 *pscalingcenter, const D3DXQUATERNION *pscalingrotation, const D3DXVECTOR3 *pscaling, const D3DXVECTOR3 *protationcenter, const D3DXQUATERNION *protation, const D3DXVECTOR3 *ptranslation);

#endif