#include "Direct3D8X.hpp"
#include "../Size.hpp"

#include <cstdint>
#include <cmath>

using namespace OSHGui::Drawing;

D3DXMATRIX* MatrixMultiply(D3DXMATRIX *pout, const D3DXMATRIX *pm1, const D3DXMATRIX *pm2)
{
	D3DXMATRIX out;
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			out.m[i][j] = pm1->m[i][0] * pm2->m[0][j] + pm1->m[i][1] * pm2->m[1][j] + pm1->m[i][2] * pm2->m[2][j] + pm1->m[i][3] * pm2->m[3][j];
		}
	}
	*pout = out;
	return pout;
}
//---------------------------------------------------------------------------
D3DXMATRIX* MatrixIdentity(D3DXMATRIX *pOut)
{
	pOut->m[0][1] = pOut->m[0][2] = pOut->m[0][3] =
	pOut->m[1][0] = pOut->m[1][2] = pOut->m[1][3] =
	pOut->m[2][0] = pOut->m[2][1] = pOut->m[2][3] =
	pOut->m[3][0] = pOut->m[3][1] = pOut->m[3][2] = 0.0f;

	pOut->m[0][0] = pOut->m[1][1] = pOut->m[2][2] = pOut->m[3][3] = 1.0f;
	return pOut;
}
//---------------------------------------------------------------------------
D3DXMATRIX* MatrixTranslation(D3DXMATRIX *pout, FLOAT x, FLOAT y, FLOAT z)
{
	MatrixIdentity(pout);
	pout->m[3][0] = x;
	pout->m[3][1] = y;
	pout->m[3][2] = z;
	return pout;
}
//---------------------------------------------------------------------------
D3DXMATRIX* MatrixRotationQuaternion(D3DXMATRIX *pout, const D3DXQUATERNION *pq)
{
	MatrixIdentity(pout);
	pout->m[0][0] = 1.0f - 2.0f * (pq->y * pq->y + pq->z * pq->z);
	pout->m[0][1] = 2.0f * (pq->x *pq->y + pq->z * pq->w);
	pout->m[0][2] = 2.0f * (pq->x * pq->z - pq->y * pq->w);
	pout->m[1][0] = 2.0f * (pq->x * pq->y - pq->z * pq->w);
	pout->m[1][1] = 1.0f - 2.0f * (pq->x * pq->x + pq->z * pq->z);
	pout->m[1][2] = 2.0f * (pq->y *pq->z + pq->x *pq->w);
	pout->m[2][0] = 2.0f * (pq->x * pq->z + pq->y * pq->w);
	pout->m[2][1] = 2.0f * (pq->y *pq->z - pq->x *pq->w);
	pout->m[2][2] = 1.0f - 2.0f * (pq->x * pq->x + pq->y * pq->y);
	return pout;
}
//---------------------------------------------------------------------------
D3DXVECTOR4* Vec4Cross(D3DXVECTOR4 *pout, const D3DXVECTOR4 *pv1, const D3DXVECTOR4 *pv2, const D3DXVECTOR4 *pv3)
{
	D3DXVECTOR4 out;
	out.x = pv1->y * (pv2->z * pv3->w - pv3->z * pv2->w) - pv1->z * (pv2->y * pv3->w - pv3->y * pv2->w) + pv1->w * (pv2->y * pv3->z - pv2->z *pv3->y);
	out.y = -(pv1->x * (pv2->z * pv3->w - pv3->z * pv2->w) - pv1->z * (pv2->x * pv3->w - pv3->x * pv2->w) + pv1->w * (pv2->x * pv3->z - pv3->x * pv2->z));
	out.z = pv1->x * (pv2->y * pv3->w - pv3->y * pv2->w) - pv1->y * (pv2->x *pv3->w - pv3->x * pv2->w) + pv1->w * (pv2->x * pv3->y - pv3->x * pv2->y);
	out.w = -(pv1->x * (pv2->y * pv3->z - pv3->y * pv2->z) - pv1->y * (pv2->x * pv3->z - pv3->x *pv2->z) + pv1->z * (pv2->x * pv3->y - pv3->x * pv2->y));
	*pout = out;
	return pout;
}
//---------------------------------------------------------------------------
float MatrixDeterminant(const D3DXMATRIX *pm)
{
	D3DXVECTOR4 minor, v1, v2, v3;

	v1.x = pm->m[0][0]; v1.y = pm->m[1][0]; v1.z = pm->m[2][0]; v1.w = pm->m[3][0];
	v2.x = pm->m[0][1]; v2.y = pm->m[1][1]; v2.z = pm->m[2][1]; v2.w = pm->m[3][1];
	v3.x = pm->m[0][2]; v3.y = pm->m[1][2]; v3.z = pm->m[2][2]; v3.w = pm->m[3][2];
	Vec4Cross(&minor, &v1, &v2, &v3);
	float det =  - (pm->m[0][3] * minor.x + pm->m[1][3] * minor.y + pm->m[2][3] * minor.z + pm->m[3][3] * minor.w);
	return det;
}
//---------------------------------------------------------------------------
D3DXMATRIX* MatrixInverse(D3DXMATRIX *pout, FLOAT *pdeterminant, const D3DXMATRIX *pm)
{
	D3DXMATRIX out;
	D3DXVECTOR4 v, vec[3];

	float det = MatrixDeterminant(pm);
	if (!det) return nullptr;
	if (pdeterminant) *pdeterminant = det;
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			if (j != i )
			{
				int a = j;
				if (j > i) a = a-1;
				vec[a].x = pm->m[j][0];
				vec[a].y = pm->m[j][1];
				vec[a].z = pm->m[j][2];
				vec[a].w = pm->m[j][3];
			}
		}
		Vec4Cross(&v, &vec[0], &vec[1], &vec[2]);
		out.m[0][i] = pow(-1.0f, i) * v.x / det;
		out.m[1][i] = pow(-1.0f, i) * v.y / det;
		out.m[2][i] = pow(-1.0f, i) * v.z / det;
		out.m[3][i] = pow(-1.0f, i) * v.w / det;
	}

	*pout = out;
	return pout;
};
//---------------------------------------------------------------------------
D3DXMATRIX* MatrixScaling(D3DXMATRIX *pout, float sx, float sy, float sz)
{
	MatrixIdentity(pout);
	pout->m[0][0] = sx;
	pout->m[1][1] = sy;
	pout->m[2][2] = sz;
	return pout;
};
//---------------------------------------------------------------------------
D3DXMATRIX* MatrixTransformation(D3DXMATRIX *pout, const D3DXVECTOR3 *pscalingcenter, const D3DXQUATERNION *pscalingrotation, const D3DXVECTOR3 *pscaling, const D3DXVECTOR3 *protationcenter, const D3DXQUATERNION *protation, const D3DXVECTOR3 *ptranslation)
{
	D3DXMATRIX m1, m2, m3, m4, m5, m6, m7;
	D3DXQUATERNION prc;
	D3DXVECTOR3 psc, pt;

	if (!pscalingcenter)
	{
		psc.x = 0.0f;
		psc.y = 0.0f;
		psc.z = 0.0f;
	}
	else
	{
		psc.x = pscalingcenter->x;
		psc.y = pscalingcenter->y;
		psc.z = pscalingcenter->z;
	}

	if (!protationcenter)
	{
		prc.x = 0.0f;
		prc.y = 0.0f;
		prc.z = 0.0f;
	}
	else
	{
		prc.x = protationcenter->x;
		prc.y = protationcenter->y;
		prc.z = protationcenter->z;
	}

	if (!ptranslation)
	{
		pt.x = 0.0f;
		pt.y = 0.0f;
		pt.z = 0.0f;
	}
	else
	{
		pt.x = ptranslation->x;
		pt.y = ptranslation->y;
		pt.z = ptranslation->z;
	}

	MatrixTranslation(&m1, -psc.x, -psc.y, -psc.z);

	if (!pscalingrotation)
	{
		MatrixIdentity(&m2);
		MatrixIdentity(&m4);
	}
	else
	{
		MatrixRotationQuaternion(&m4, pscalingrotation);
		MatrixInverse(&m2, nullptr, &m4);
	}

	if (!pscaling) MatrixIdentity(&m3);
	else MatrixScaling(&m3, pscaling->x, pscaling->y, pscaling->z);

	if (!protation) MatrixIdentity(&m6);
	else MatrixRotationQuaternion(&m6, protation);

	MatrixTranslation(&m5, psc.x - prc.x,  psc.y - prc.y,  psc.z - prc.z);
	MatrixTranslation(&m7, prc.x + pt.x, prc.y + pt.y, prc.z + pt.z);

	MatrixMultiply(&m1, &m1, &m2);
	MatrixMultiply(&m1, &m1, &m3);
	MatrixMultiply(&m1, &m1, &m4);
	MatrixMultiply(&m1, &m1, &m5);
	MatrixMultiply(&m1, &m1, &m6);
	MatrixMultiply(pout, &m1, &m7);

	return pout;
}
//---------------------------------------------------------------------------
D3DXVECTOR3* Vec3Normalize(D3DXVECTOR3 *pout, const D3DXVECTOR3 *pv)
{
	auto norm = D3DXVec3Length(pv);
	if ( !norm )
	{
		pout->x = 0.0f;
		pout->y = 0.0f;
		pout->z = 0.0f;
	}
	else
	{
		pout->x = pv->x / norm;
		pout->y = pv->y / norm;
		pout->z = pv->z / norm;
	}

	return pout;
}
//---------------------------------------------------------------------------
D3DXMATRIX* MatrixLookAtRH(D3DXMATRIX *out, const D3DXVECTOR3 *eye, const D3DXVECTOR3 *at, const D3DXVECTOR3 *up)
{
	D3DXVECTOR3 right, upn, vec;

	D3DXVec3Subtract(&vec, at, eye);
	Vec3Normalize(&vec, &vec);
	D3DXVec3Cross(&right, up, &vec);
	D3DXVec3Cross(&upn, &vec, &right);
	Vec3Normalize(&right, &right);
	Vec3Normalize(&upn, &upn);
	out->m[0][0] = -right.x;
	out->m[1][0] = -right.y;
	out->m[2][0] = -right.z;
	out->m[3][0] = D3DXVec3Dot(&right, eye);
	out->m[0][1] = upn.x;
	out->m[1][1] = upn.y;
	out->m[2][1] = upn.z;
	out->m[3][1] = -D3DXVec3Dot(&upn, eye);
	out->m[0][2] = -vec.x;
	out->m[1][2] = -vec.y;
	out->m[2][2] = -vec.z;
	out->m[3][2] = D3DXVec3Dot(&vec, eye);
	out->m[0][3] = 0.0f;
	out->m[1][3] = 0.0f;
	out->m[2][3] = 0.0f;
	out->m[3][3] = 1.0f;

	return out;
}
//---------------------------------------------------------------------------
D3DXMATRIX* MatrixPerspectiveFovRH(D3DXMATRIX *pout, float fovy, float aspect, float zn, float zf)
{
	MatrixIdentity(pout);
	pout->m[0][0] = 1.0f / (aspect * std::tan(fovy/2.0f));
	pout->m[1][1] = 1.0f / std::tan(fovy/2.0f);
	pout->m[2][2] = zf / (zn - zf);
	pout->m[2][3] = -1.0f;
	pout->m[3][2] = (zf * zn) / (zn - zf);
	pout->m[3][3] = 0.0f;
	return pout;
}
//---------------------------------------------------------------------------
D3DXMATRIX* MatrixOrthoOffCenterRH(D3DXMATRIX *pout, float l, float r, float b, float t, float zn, float zf)
{
	MatrixIdentity(pout);
	pout->m[0][0] = 2.0f / (r - l);
	pout->m[1][1] = 2.0f / (t - b);
	pout->m[2][2] = 1.0f / (zn -zf);
	pout->m[3][0] = -1.0f -2.0f *l / (r - l);
	pout->m[3][1] = 1.0f + 2.0f * t / (b - t);
	pout->m[3][2] = zn / (zn -zf);
	return pout;
}
//---------------------------------------------------------------------------
enum class FormatType
{
	ARGB,
	DXT,
	UNKNOWN
};

struct PixelFormatDescription
{
	D3DFORMAT format;
	BYTE bits[4];
	BYTE shift[4];
	UINT bytesPerPixel;
	UINT blockWidth;
	UINT blockHeight;
	UINT blockByteCount;
	FormatType type;
} formats[] =
{
	{D3DFMT_R8G8B8,        { 0,  8,  8,  8}, { 0, 16,  8,  0},  3, 1, 1,  3, FormatType::ARGB},
	{D3DFMT_A8R8G8B8,      { 8,  8,  8,  8}, {24, 16,  8,  0},  4, 1, 1,  4, FormatType::ARGB},
	{D3DFMT_X8R8G8B8,      { 0,  8,  8,  8}, { 0, 16,  8,  0},  4, 1, 1,  4, FormatType::ARGB},
	{D3DFMT_R5G6B5,        { 0,  5,  6,  5}, { 0, 11,  5,  0},  2, 1, 1,  2, FormatType::ARGB},
	{D3DFMT_X1R5G5B5,      { 0,  5,  5,  5}, { 0, 10,  5,  0},  2, 1, 1,  2, FormatType::ARGB},
	{D3DFMT_A1R5G5B5,      { 1,  5,  5,  5}, {15, 10,  5,  0},  2, 1, 1,  2, FormatType::ARGB},
	{D3DFMT_R3G3B2,        { 0,  3,  3,  2}, { 0,  5,  2,  0},  1, 1, 1,  1, FormatType::ARGB},
	{D3DFMT_A8R3G3B2,      { 8,  3,  3,  2}, { 8,  5,  2,  0},  2, 1, 1,  2, FormatType::ARGB},
	{D3DFMT_A4R4G4B4,      { 4,  4,  4,  4}, {12,  8,  4,  0},  2, 1, 1,  2, FormatType::ARGB},
	{D3DFMT_X4R4G4B4,      { 0,  4,  4,  4}, { 0,  8,  4,  0},  2, 1, 1,  2, FormatType::ARGB},
	{D3DFMT_A2B10G10R10,   { 2, 10, 10, 10}, {30,  0, 10, 20},  4, 1, 1,  4, FormatType::ARGB},
	{D3DFMT_G16R16,        { 0, 16, 16,  0}, { 0,  0, 16,  0},  4, 1, 1,  4, FormatType::ARGB},
	{D3DFMT_A8,            { 8,  0,  0,  0}, { 0,  0,  0,  0},  1, 1, 1,  1, FormatType::ARGB},
	{D3DFMT_DXT1,          { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4,  8, FormatType::DXT},
	{D3DFMT_DXT2,          { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4, 16, FormatType::DXT},
	{D3DFMT_DXT3,          { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4, 16, FormatType::DXT},
	{D3DFMT_DXT4,          { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4, 16, FormatType::DXT},
	{D3DFMT_DXT5,          { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4, 16, FormatType::DXT},
	{D3DFMT_UNKNOWN,       { 0,  0,  0,  0}, { 0,  0,  0,  0},  0, 1, 1,  0, FormatType::UNKNOWN}
};
//---------------------------------------------------------------------------
const PixelFormatDescription& GetFormatInfo(D3DFORMAT format)
{
	auto i = 0;
	for (; formats[i].format != format && formats[i].format != D3DFMT_UNKNOWN; ++i);
	if (formats[i].format == D3DFMT_UNKNOWN)
	{
		throw;
	}
	return formats[i];
}
//---------------------------------------------------------------------------
void CopyPixels(const BYTE *src, UINT srcRowPitch, UINT src_slice_pitch, BYTE *dst, UINT destRowPitch, UINT dst_slice_pitch, const SizeI &size, const PixelFormatDescription &format)
{
	auto rowBlockCount = (size.Width + format.blockWidth - 1) / format.blockWidth;
	auto rowCount = (size.Height + format.blockHeight - 1) / format.blockHeight;

	for (auto row = 0u; row < rowCount; ++row)
	{
		std::memcpy(dst, src, rowBlockCount * format.blockByteCount);
		src += srcRowPitch;
		dst += destRowPitch;
	}
}
//---------------------------------------------------------------------------
HRESULT LoadSurfaceFromMemory(IDirect3DSurface8 *destSurface, const PALETTEENTRY *destPaletteEntry, const RECT *destRect, const void *srcData, D3DFORMAT format, UINT srcPitch, const PALETTEENTRY *srcPaletteEntry, const RECT *srcRect, DWORD filter, D3DCOLOR color)
{
	if (!destSurface || !srcData || !srcRect)
	{
		return D3DERR_INVALIDCALL;
	}
	if (format == D3DFMT_UNKNOWN || srcRect->left >= srcRect->right || srcRect->top >= srcRect->bottom)
	{
		return E_FAIL;
	}

	if (filter == D3DX_DEFAULT)
	{
		filter = D3DX_FILTER_TRIANGLE | D3DX_FILTER_DITHER;
	}

	D3DSURFACE_DESC surfdesc;
	destSurface->GetDesc(&surfdesc);

	SizeI size1(srcRect->right - srcRect->left, srcRect->bottom - srcRect->top);
	SizeI size2;
	if (!destRect)
	{
		size2.Width = surfdesc.Width;
		size2.Height = surfdesc.Height;
	}
	else
	{
		if (destRect->left > destRect->right || destRect->right > surfdesc.Width || destRect->top > destRect->bottom || destRect->bottom > surfdesc.Height || destRect->left < 0 || destRect->top < 0)
		{
			return D3DERR_INVALIDCALL;
		}
		size2.Width = destRect->right - destRect->left;
		size2.Height = destRect->bottom - destRect->top;
		if (size2.Width == 0 || size2.Height == 0)
		{
			return D3D_OK;
		}
	}

	auto &srcformatdesc = GetFormatInfo(format);
	auto &destformatdesc = GetFormatInfo(surfdesc.Format);
	if (srcformatdesc.type == FormatType::UNKNOWN || destformatdesc.type == FormatType::UNKNOWN)
	{
		return E_NOTIMPL;
	}

	if (format == surfdesc.Format && size2.Width == size1.Width && size2.Height == size1.Height && color == 0)
	{
		if (srcRect->left & (srcformatdesc.blockWidth - 1)
			|| srcRect->top & (srcformatdesc.blockHeight - 1)
			|| (srcRect->right & (srcformatdesc.blockWidth - 1)
			&& size1.Width != surfdesc.Width)
			|| (srcRect->bottom & (srcformatdesc.blockHeight - 1)
			&& size1.Height != surfdesc.Height))
		{
			return D3DXERR_INVALIDDATA;
		}

		D3DLOCKED_RECT lock;
		if (FAILED(destSurface->LockRect(&lock, destRect, 0)))
		{
			return D3DXERR_INVALIDDATA;
		}

		CopyPixels(static_cast<const uint8_t*>(srcData), srcPitch, 0, static_cast<uint8_t*>(lock.pBits), lock.Pitch, 0, size1, srcformatdesc);

		destSurface->UnlockRect();
	}
	else
	{
		return E_NOTIMPL;
	}

	return D3D_OK;
}
//---------------------------------------------------------------------------