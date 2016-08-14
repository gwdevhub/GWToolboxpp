#include "Minimap.h"

#include <Windows.h>
#include <windowsx.h>
#include <thread>
#include <d3d9.h>
#include <d3dx9math.h>

#include <GWCA\GWCA.h>
#include <GWCA\StoCMgr.h>
#include <GWCA\CameraMgr.h>

//void Minimap::UIRenderer::Initialize(IDirect3DDevice9* device) {
//	count_ = 1;
//	type_ = D3DPT_TRIANGLEFAN;
//	printf("initializing minimap\n");
//	D3DVertex* vertices = nullptr;
//	unsigned int vertex_count = 4;
//
//	if (buffer_) buffer_->Release();
//	device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, 0,
//		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer_, NULL);
//	buffer_->Lock(0, sizeof(D3DVertex) * vertex_count,
//		(VOID**)&vertices, D3DLOCK_DISCARD);
//
//	for (unsigned int i = 0; i < vertex_count; ++i) {
//		vertices[i].z = 0.0f;
//		vertices[i].color = D3DCOLOR_ARGB(0x77, 0xFF, 0xFF, 0xFF);
//	}
//
//	vertices[0].x = -1;
//	vertices[0].y = 1;
//	
//	vertices[1].x = -1;
//	vertices[1].y = -1;
//
//	vertices[2].x = 1;
//	vertices[2].y = -1;
//
//	vertices[3].x = 1;
//	vertices[3].y = 1;
//
//	buffer_->Unlock();
//}

Minimap::Minimap() 
	: range_renderer(RangeRenderer()),
	pmap_renderer(PmapRenderer()),
	agent_renderer(AgentRenderer()),
	dragging_(false) {

	GWCA::StoC().AddGameServerEvent<GWCA::StoC_Pak::P391_InstanceLoadFile>(
		[this](GWCA::StoC_Pak::P391_InstanceLoadFile* packet) {
		printf("loading map %d\n", packet->map_fileID);
		pmap_renderer.Invalidate();
		return false;
	});

	pmap_renderer.Invalidate();
}

void Minimap::Render(IDirect3DDevice9* device) {
	using namespace GWCA;
	if (GWCA::Map().GetInstanceType() == GwConstants::InstanceType::Loading) {
		return;
	}

	GW::Agent* me = GWCA::Agents().GetPlayer();
	if (me != nullptr) {
		SetTranslation(-me->X, -me->Y);
	}
	float camera_yaw = GWCA::Camera().GetYaw();
	SetRotation(-camera_yaw + (float)M_PI_2);

	device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);

	D3DXMATRIX identity;
	D3DXMatrixIdentity(&identity);
	device->SetTransform(D3DTS_WORLD, &identity);
	device->SetTransform(D3DTS_VIEW, &identity);
	device->SetTransform(D3DTS_PROJECTION, &identity);

	RenderSetupClipping(device);
	RenderSetupProjection(device);
	
	RenderSetupWorldTransforms(device);
	pmap_renderer.Render(device);	

	D3DXMATRIX scale;
	D3DXMatrixScaling(&scale, scale_, scale_, 1);
	device->SetTransform(D3DTS_VIEW, &scale);
	device->SetTransform(D3DTS_WORLD, &identity);
	range_renderer.Render(device);

	RenderSetupWorldTransforms(device);
	agent_renderer.Render(device);

	pingslines_renderer.Render(device);
}


bool Minimap::OnMouseDown(MSG msg) {
	int x = GET_X_LPARAM(msg.lParam);
	int y = GET_Y_LPARAM(msg.lParam);
	if (x > GetX() && x < GetX() + GetWidth()
		&& y > GetY() && y < GetY() + GetHeight()) {
		drag_start_x_ = x;
		drag_start_y_ = y;
		dragging_ = true;
		return true;
	}
	return false;
}

bool Minimap::OnMouseUp(MSG msg) {
	dragging_ = false;
	return false;
}

bool Minimap::OnMouseMove(MSG msg) {
	int x = GET_X_LPARAM(msg.lParam);
	int y = GET_Y_LPARAM(msg.lParam);
	if (dragging_) {
		int diff_x = x - drag_start_x_;
		int diff_y = y - drag_start_y_;
		SetX(GetX() + diff_x);
		SetY(GetY() + diff_y);
		drag_start_x_ = x;
		drag_start_y_ = y;
		return true;
	}
	return false;
}

bool Minimap::OnMouseWheel(MSG msg) {
	return false;
}