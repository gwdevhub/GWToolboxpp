#include "Minimap.h"

#include <Windows.h>
#include <windowsx.h>

#include "GWCA/APIMain.h"

void Minimap::UIRenderer::Initialize(IDirect3DDevice9* device) {
	count_ = 2;
	type_ = D3DPT_TRIANGLEFAN;

	Vertex* vertices = nullptr;
	unsigned int vertex_count = 4;

	if (buffer_) buffer_->Release();
	device->CreateVertexBuffer(sizeof(Vertex) * vertex_count, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer_, NULL);
	buffer_->Lock(0, sizeof(Vertex) * vertex_count,
		(VOID**)&vertices, D3DLOCK_DISCARD);

	for (unsigned int i = 0; i < vertex_count; ++i) {
		vertices[i].z = 0;
		vertices[i].color = D3DCOLOR_ARGB(0xAA, 0x22, 0x22, 0x22);
	}

	vertices[0].x = -1;
	vertices[0].y = 1;
	
	vertices[1].x = -1;
	vertices[1].y = -1;

	vertices[2].x = 1;
	vertices[2].y = -1;

	vertices[3].x = 1;
	vertices[3].y = 1;

	buffer_->Unlock();
}

void Minimap::RangeRenderer::CreateCircle(Vertex* vertices, float radius) {
	for (size_t i = 0; i < circle_vertices; ++i) {
		float angle = i * (2 * static_cast<float>(M_PI) / circle_vertices);
		vertices[i].x = radius * std::cos(angle);
		vertices[i].y = radius * std::sin(angle);
		vertices[i].z = 1.0f;
		vertices[i].color = 0xFF666677;
	}
	vertices[circle_points] = vertices[0];
}
void Minimap::RangeRenderer::Initialize(IDirect3DDevice9* device) {
	count_ = circle_points * 3; // radar range, spirit range, aggro range
	type_ = D3DPT_LINESTRIP;
	float radius;

	Vertex* vertices = nullptr;
	unsigned int vertex_count = count_ + 3;

	if (buffer_) buffer_->Release();
	device->CreateVertexBuffer(sizeof(Vertex) * vertex_count, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer_, NULL);
	buffer_->Lock(0, sizeof(Vertex) * vertex_count,
		(VOID**)&vertices, D3DLOCK_DISCARD);
	
	radius = static_cast<float>(GwConstants::Range::Compass);
	CreateCircle(vertices + circle_vertices * 0, radius);

	radius = static_cast<float>(GwConstants::Range::Spirit);
	CreateCircle(vertices + circle_vertices * 1, radius);

	radius = static_cast<float>(GwConstants::Range::Spellcast);
	CreateCircle(vertices + circle_vertices * 2, radius);

	buffer_->Unlock();
}
void Minimap::RangeRenderer::Render(IDirect3DDevice9* device) {
	if (!initialized_) {
		Initialize(device);
		initialized_ = true;
	}

	device->SetFVF(D3DFVF_CUSTOMVERTEX);
	device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	device->SetRenderState(D3DRS_LIGHTING, FALSE);

	device->SetStreamSource(0, buffer_, 0, sizeof(Vertex));
	device->DrawPrimitive(type_, circle_vertices * 0, circle_points);
	device->DrawPrimitive(type_, circle_vertices * 1, circle_points);
	device->DrawPrimitive(type_, circle_vertices * 2, circle_points);
}

Minimap::Minimap() 
	: ui_renderer(UIRenderer()), 
	range_renderer(RangeRenderer()),
	pmap_renderer(PmapRenderer()),
	agent_renderer(AgentRenderer()) {

	GWAPI::GWCA api;
	api().StoC().AddGameServerEvent<GWAPI::StoC::P391_InstanceLoadFile>(
		[this](GWAPI::StoC::P391_InstanceLoadFile* packet) {
		pmap_renderer.LoadMap(packet->map_fileID);
	});

	pmap_renderer.LoadMap(219215);
}

void Minimap::Render(IDirect3DDevice9* device) {
	using namespace GWAPI;
	GWCA api;
	GW::Agent* me = api().Agents().GetPlayer();
	if (me != nullptr) {
		SetTranslation(-me->X, -me->Y);
	}
	float camera_yaw = api().Camera().GetYaw();
	SetRotation(-camera_yaw + (float)M_PI_2);

	device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);

	RenderSetupClipping(device);
	RenderSetupViewport(device);

	RenderSetupIdentityTransforms(device);
	ui_renderer.Render(device);

	RenderSetupWorldTransforms(device);
	device->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_MAX);
	pmap_renderer.Render(device);

	device->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
	RenderSetupIdentityTransforms(device);
	D3DXMATRIX scale;
	float ratio = (float)GetWidth() / GetHeight();
	D3DXMatrixScaling(&scale, scale_, scale_, 1);
	device->SetTransform(D3DTS_VIEW, &scale);
	range_renderer.Render(device);

	RenderSetupWorldTransforms(device);
	agent_renderer.Render(device);
}


bool Minimap::OnMouseDown(MSG msg) {
	int x = GET_X_LPARAM(msg.lParam);
	int y = GET_Y_LPARAM(msg.lParam);
	if (x > GetX() && x < GetX() + GetWidth()
		&& y > GetY() && y < GetY() + GetHeight()) {
		drag_start_ = Point2i(x, y);
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
		Point2i diff = Point2i(x, y) - drag_start_;
		SetX(GetX() + diff.x());
		SetY(GetY() + diff.y());
		drag_start_ = Point2i(x, y);
		return true;
	}
	return false;
}

bool Minimap::OnMouseWheel(MSG msg) {
	return false;
}