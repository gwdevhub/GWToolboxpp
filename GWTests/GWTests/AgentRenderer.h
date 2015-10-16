#pragma once

#include "Renderer.h"
#include <GWCA\APIMain.h>

class AgentRenderer : public Renderer {
public:
	void Render(IDirect3DDevice9* device) override;

private:
	void Initialize(IDirect3DDevice9* device) override;
	void QueueTriangle(IDirect3DDevice9* device, 
		float x, float y, float rotation, float size, DWORD color);
	void QueueQuad(IDirect3DDevice9* device,
		float x, float y, float size, DWORD color);
	void Flush(IDirect3DDevice9* device);

	Vertex* vertices;			// vertices array
	unsigned int triangle_count;// count of triangles
	unsigned int vertices_max;	// max number of vertices to draw in one call
	unsigned int triangles_max; // max number of triangles to draw in one call
};
