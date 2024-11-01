#pragma once

#include <DebugDraw.h>
#include <Recast.h>

#include "../../Minimap/D3DVertex.h"

class DebugDrawDX9 : public duDebugDraw
{
    std::vector<D3DVertex> verts;
    std::vector<unsigned int> indi;
    D3DPRIMITIVETYPE prim_type = D3DPT_POINTLIST;
    bool quad_flag = false;

public:
	virtual void depthMask(bool state);
	virtual void texture(bool state);
	virtual void begin(duDebugDrawPrimitives prim, float size = 1.0f);
	virtual void vertex(const float* pos, unsigned int color);
	virtual void vertex(const float x, const float y, const float z, unsigned int color);
	virtual void vertex(const float* pos, unsigned int color, const float* uv);
	virtual void vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v);
	virtual void end();
};

#define GL_TRUE 1
#define GL_FALSE 0

#define GL_FOG 11
#define GL_LINES 22

#define GLdouble double

void glEnable(int i);
void glDisable(int i);
void glDepthMask(int i); //v
void glColor4ub(int r, int g, int b, int a); //v
void glLineWidth(float w); //v
void glBegin(int i); //v lines
void glVertex3f(float x, float y, float z); //v
void glEnd(); //v

int gluProject(double objx, double objy, double objz,
          const double modelMatrix[16],
          const double projMatrix[16],
              const int viewport[4],
          double *winx, double *winy, double *winz);
