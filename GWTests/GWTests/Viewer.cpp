#include "Viewer.h"
#include "GWCA/APIMain.h"

Viewer::Viewer() {
	location_ = Point2i(0, 0);
	size_ = Point2i(300, 300);
	translation_x_ = 0.0f;
	translation_y_ = 0.0f;
	scale_ = 1.0f;
	rotation_ = 0.0f;
}

void Viewer::RenderSetup(IDirect3DDevice9* device) {
	RenderSetupClipping(device);
	RenderSetupViewport(device);
	RenderSetupWorldTransforms(device);
}

void Viewer::RenderSetupClipping(IDirect3DDevice9* device) {
	RECT clipping;
	clipping.left = location_.x();
	clipping.right = location_.x() + size_.x();
	clipping.top = location_.y();
	clipping.bottom = location_.y() + size_.y();
	device->SetScissorRect(&clipping);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
}

void Viewer::RenderSetupViewport(IDirect3DDevice9* device) {
	D3DVIEWPORT9 viewport;
	device->GetViewport(&viewport);
	viewport.X = location_.x();
	viewport.Y = location_.y();
	if (size_.x() > 0) viewport.Width = size_.x();
	if (size_.y() > 0) viewport.Height = size_.y();
	device->SetViewport(&viewport);
}

void Viewer::RenderSetupIdentityTransforms(IDirect3DDevice9* device) {
	D3DXMATRIX identity;
	D3DXMatrixIdentity(&identity);
	device->SetTransform(D3DTS_PROJECTION, &identity);
	device->SetTransform(D3DTS_WORLD, &identity);
	device->SetTransform(D3DTS_VIEW, &identity);
}

void Viewer::RenderSetupWorldTransforms(IDirect3DDevice9* device) {

	D3DXMATRIX world;
	D3DXMatrixIdentity(&world);
	device->SetTransform(D3DTS_WORLD, &world);

	D3DXMATRIX translate;
	D3DXMatrixTranslation(&translate, translation_x_, translation_y_, 0);

	D3DXMATRIX rotate;
	D3DXMatrixRotationZ(&rotate, rotation_);

	D3DXMATRIX scale;
	float ratio = (float)GetWidth() / GetHeight();
	D3DXMatrixScaling(&scale, scale_, scale_ * ratio, 1);

	D3DXMATRIX view = translate * rotate * scale;
	device->SetTransform(D3DTS_VIEW, &view);

	D3DXMATRIX projection;
	D3DXMatrixOrthoLH(&projection, 2, 2, 0.5f, 1.5f);
	device->SetTransform(D3DTS_PROJECTION, &projection);
}

