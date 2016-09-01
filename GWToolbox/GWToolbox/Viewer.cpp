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

void Viewer::RenderSetupWorldTransforms(IDirect3DDevice9* device,
	bool translate, bool rotate, bool scale) {

	// everything is already in world coordinates, no need to change
	D3DXMATRIX world;
	D3DXMatrixIdentity(&world);
	device->SetTransform(D3DTS_WORLD, &world);

	// set up view
	// translate by character position (still in world coordinates)
	D3DXMATRIX m_translate;
	if (translate) {
		D3DXMatrixTranslation(&m_translate, translation_x_, translation_y_, 0);
	} else {
		D3DXMatrixIdentity(&m_translate);
	}

	// rotate for character rotation
	D3DXMATRIX m_rotate;
	if (rotate) {
		D3DXMatrixRotationZ(&m_rotate, rotation_);
	} else {
		D3DXMatrixIdentity(&m_rotate);
	}
	

	// scale for minimap zoom
	D3DXMATRIX m_scale;
	if (scale) {
		//D3DXMatrixScaling(&m_scale, scale_, scale_, 1);
		D3DXMatrixIdentity(&m_scale);
	} else {
		D3DXMatrixIdentity(&m_scale);
	}
	
	D3DXMATRIX view = m_translate * m_rotate * m_scale;
	device->SetTransform(D3DTS_VIEW, &view);

	//if (translate && rotate && scale) {
	//	printf("======\n");
	//	printf("translation x %f, y %f\n", translation_x_, translation_y_);
	//	printf("rotation %f, sin %f, cos %f\n", rotation_, std::sin(rotation_), std::cos(rotation_));
	//	printf("scale: %f\n", scale_);
	//	for (int row = 0; row < 4; ++row) {
	//		for (int col = 0; col < 4; ++col) {
	//			printf("%f, ", view(row, col));
	//		}
	//		printf("\n");
	//	}
	//}
}
void Viewer::RenderSetupProjection(IDirect3DDevice9* device) {
	D3DVIEWPORT9 viewport;
	device->GetViewport(&viewport);

	//// scale from screen size to minimap size
	//float ratio = (float)width_ / height_;
	//D3DXMATRIX scale;
	//D3DXMatrixScaling(&scale, (float)width_ / viewport.Width, 
	//	(float)height_ / viewport.Height * ratio, 1);

	//D3DXMATRIX translate;
	//D3DXMatrixTranslation(&translate, -1.0f + (float)(location_x_ * 2 + width_) / viewport.Width, 
	//	1.0f - (float)(location_y_ * 2 + height_) / viewport.Height, 0.0f);

	//D3DXMATRIX projection = scale * translate;

	float w = 5000.0f * 2;
	// IMPORTANT: we are setting z-near to 0.0f and z-far to 1.0f
	D3DXMATRIX orth_proj_matrix(
		2 / w, 0, 0, 0,
		0, 2 / w, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);
	//D3DXMatrixOrthoLH(&orth_proj_matrix, w, w, 0.5f, 1.5f);

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

	D3DXMATRIX proj = orth_proj_matrix * viewport_matrix;

	device->SetTransform(D3DTS_PROJECTION, &proj);
}
