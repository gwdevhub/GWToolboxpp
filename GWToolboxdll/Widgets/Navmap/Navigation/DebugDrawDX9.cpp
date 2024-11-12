#include "stdafx.h"
#include "DebugDrawDX9.h"
#include <RecastDebugDraw.h>
#include <DetourDebugDraw.h>
#include "RecastDemo/SDL_opengl.h"
#include "../NavGameWorldRenderer.h"

//extern volatile bool navmesh_building;
extern NavGameWorldRenderer * navgwr;

void DebugDrawDX9::depthMask(bool state)
{
    if(!navgwr || !navgwr->dx9_device)return;
	navgwr->dx9_device->SetRenderState(D3DRS_ZENABLE, state ? D3DZB_TRUE : D3DZB_FALSE);
}

void DebugDrawDX9::texture(bool state)
{
    (void)state;
}

void DebugDrawDX9::begin(duDebugDrawPrimitives prim, float size)
{
    (void)size;

    verts.clear();

	switch(prim)
	{
		case DU_DRAW_POINTS:
            quad_flag = false;
			//glPointSize(size);
			prim_type = D3DPT_POINTLIST;
			break;
		case DU_DRAW_LINES:
            quad_flag = false;
			//glLineWidth(size);
			prim_type = D3DPT_LINELIST;
			break;
		case DU_DRAW_TRIS:
            quad_flag = false;
			prim_type = D3DPT_TRIANGLELIST;
			break;
		case DU_DRAW_QUADS:
            quad_flag = true;
			prim_type = D3DPT_TRIANGLELIST;
			break;
	};
}

#define RGBA_TO_ARGB(color) \
    ((((color) & 0xFF) << 24) | (((color) >> 16) & 0xFF) | (((color) >> 8) & 0xFF00) | ((color) << 16))

void DebugDrawDX9::vertex(const float* pos, unsigned int color)
{
    verts.push_back({pos[0], pos[1], pos[2], RGBA_TO_ARGB(color)});
}

void DebugDrawDX9::vertex(const float x, const float y, const float z, unsigned int color)
{
	verts.push_back({x, y, z, RGBA_TO_ARGB(color)});
}

void DebugDrawDX9::vertex(const float* pos, unsigned int color, const float* uv)
{
    (void)uv;
	verts.push_back({pos[0], pos[1], pos[2], RGBA_TO_ARGB(color)});
}

void DebugDrawDX9::vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v)
{
    (void)u;
    (void)v;
	verts.push_back({x, y, z, RGBA_TO_ARGB(color)});
}

void DebugDrawDX9::end()
{
    if(!navgwr || !navgwr->dx9_device)return;

	if(quad_flag)
    {
        size_t numQuads = verts.size() / 4;
        size_t numIndices = numQuads * 6;

        indi.clear();
        indi.reserve(numIndices);

        for(size_t i = 0; i < verts.size(); i += 4)
        {
            unsigned int baseIndex = static_cast<unsigned int>(indi.size());

            indi.push_back(baseIndex + 0);
            indi.push_back(baseIndex + 1);
            indi.push_back(baseIndex + 2);

            indi.push_back(baseIndex + 0);
            indi.push_back(baseIndex + 2);
            indi.push_back(baseIndex + 3);
        }

        navgwr->dx9_device->DrawIndexedPrimitiveUP(prim_type, 0, verts.size(), numQuads * 2, indi.data(), D3DFMT_INDEX32, verts.data(), sizeof(D3DVertex));
    }
    else
    {
        if(prim_type == D3DPT_POINTLIST)
        {
            navgwr->dx9_device->DrawPrimitiveUP(prim_type, verts.size(), verts.data(), sizeof(D3DVertex));
        }
        else if(prim_type == D3DPT_LINELIST)
        {
            navgwr->dx9_device->DrawPrimitiveUP(prim_type, verts.size() / 2, verts.data(), sizeof(D3DVertex));
        }
        else if(prim_type == D3DPT_TRIANGLELIST)
        {
            navgwr->dx9_device->DrawPrimitiveUP(prim_type, verts.size() / 3, verts.data(), sizeof(D3DVertex));
        }
    }
     
	//glLineWidth(1.0f);
	//glPointSize(1.0f);
}

void glEnable(int i)
{
    (void)i;
}

void glDisable(int i)
{
    (void)i;
}

void glDepthMask(int i)
{
    (void)i;
}

void glColor4ub(int r, int g, int b, int a)
{
    (void)r; (void)g; (void)b; (void)a;
}

void glLineWidth(float w)
{
    (void)w;
}

void glBegin(int i)
{
    (void)i;
}

void glVertex3f(float x, float y, float z)
{
    (void)x; (void)y; (void)z;
}

void glEnd()
{

}

int gluProject(double objx, double objy, double objz,
          const double modelMatrix[16],
          const double projMatrix[16],
              const int viewport[4],
          double *winx, double *winy, double *winz)
{
    (void)objx;
    (void)objy;
    (void)objz;
    (void)modelMatrix;
    (void)projMatrix;
    (void)viewport;
    (void)winx;
    (void)winy;
    (void)winz;

	return 0;
}
