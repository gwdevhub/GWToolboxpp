#include "Viewer.h"

#include <d3dx9math.h>
#include <stdio.h>
#include <cmath>

Viewer::Viewer() {
	location_x_ = 0;
	location_y_ = 0;
	width_ = 300;
	height_ = 300;
	translation_x_ = 0.0f;
	translation_y_ = 0.0f;
	scale_ = 1.0f;
}

void Viewer::RenderSetupClipping(IDirect3DDevice9* device) {
	RECT clipping;
	clipping.left = location_x_ < 0 ? 0 : location_x_;
	clipping.right = location_x_ + width_ + 1;
	clipping.top = location_y_ < 0 ? 0 : location_y_;
	clipping.bottom = location_y_ + height_ + 1;
	device->SetScissorRect(&clipping);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
}


void Viewer::RenderSetupProjection(IDirect3DDevice9* device) {
	D3DVIEWPORT9 viewport;
	device->GetViewport(&viewport);

	float w = 5000.0f * 2;
	// IMPORTANT: we are setting z-near to 0.0f and z-far to 1.0f
	D3DXMATRIX ortho_matrix(
		2 / w, 0, 0, 0,
		0, 2 / w, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);

	//// note: manually craft the projection to viewport instead of using
	//// SetViewport to allow target regions outside the viewport 
	//// e.g. negative x/y for slightly offscreen map
	float xscale = (float)width_ / viewport.Width;
	float yscale = (float)height_ / viewport.Height;
	float xtrans = (float)(location_x_ + location_x_ + width_) / viewport.Width - 1.0f;
	float ytrans = -(float)(location_y_ + location_y_ + height_) / viewport.Height + 1.0f;
	////IMPORTANT: we are basically setting z-near to 0 and z-far to 1
	D3DXMATRIX viewport_matrix(
		xscale, 0, 0, 0,
		0, yscale, 0, 0,
		0, 0, 1, 0,
		xtrans, ytrans, 0, 1
	);

	D3DXMATRIX proj = ortho_matrix * viewport_matrix;

	device->SetTransform(D3DTS_PROJECTION, &proj);
}
