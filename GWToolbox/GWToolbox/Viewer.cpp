#include "Viewer.h"

#include <d3dx9math.h>
#include <stdio.h>

Viewer::Viewer() {
	location_x_ = 0;
	location_y_ = 0;
	width_ = 300;
	height_ = 300;
	translation_x_ = 0.0f;
	translation_y_ = 0.0f;
	scale_ = 1.0f;
	rotation_ = 0.0f;
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

void Viewer::RenderSetupWorldTransforms(IDirect3DDevice9* device) {

	// everything is already in world coordinates, no need to change
	D3DXMATRIX world;
	D3DXMatrixIdentity(&world);
	device->SetTransform(D3DTS_WORLD, &world);

	// set up view
	// translate by character position (still in world coordinates)
	D3DXMATRIX translate;
	D3DXMatrixTranslation(&translate, translation_x_, translation_y_, 0);

	// rotate for character rotation
	D3DXMATRIX rotate;
	D3DXMatrixRotationZ(&rotate, rotation_);

	// scale for minimap zoom
	D3DXMATRIX zoom;
	float ratio = (float)GetWidth() / GetHeight();
	D3DXMatrixScaling(&zoom, scale_, scale_ /** ratio*/, 1);

	D3DXMATRIX view = translate * rotate * zoom;
	device->SetTransform(D3DTS_VIEW, &view);
}
void Viewer::RenderSetupProjection(IDirect3DDevice9* device) {
	D3DVIEWPORT9 viewport;
	device->GetViewport(&viewport);

	// scale from screen size to minimap size
	float ratio = (float)width_ / height_;
	D3DXMATRIX scale;
	D3DXMatrixScaling(&scale, (float)width_ / viewport.Width, 
		(float)height_ / viewport.Height * ratio, 1);

	D3DXMATRIX translate;
	D3DXMatrixTranslation(&translate, -1.0f + (float)(location_x_ * 2 + width_) / viewport.Width, 
		1.0f - (float)(location_y_ * 2 + height_) / viewport.Height, 0.0f);

	D3DXMATRIX projection = scale * translate;
	//D3DXMatrixOrthoLH(&projection, 2.0f, 2.0f, 0.5f, 1.5f);
	//D3DXMatrixIdentity(&projection);
	device->SetTransform(D3DTS_PROJECTION, &projection);
}
