#include "PmapRenderer.h"

#include <vector>
#include <d3dx9math.h>
#include <d3d9.h>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\CameraMgr.h>

#include "D3DVertex.h"
#include "Config.h"

PmapRenderer::PmapRenderer() : VBuffer() {
	auto IniReadColor = [](wchar_t* key, wchar_t* def) -> DWORD {
		const wchar_t* wc = Config::IniRead(L"minimap", key, def);
		Config::IniWrite(L"minimap", key, wc);
		DWORD c = std::wcstoul(wc, nullptr, 16);
		if (c == LONG_MAX) return D3DCOLOR_ARGB(0xFF, 0x0, 0x0, 0x0);
		return c;
	};

	color_map = IniReadColor(L"color_map", L"0xFF999999");
	color_mapshadow = IniReadColor(L"color_mapshadow", L"0xFF120808");
	shadow_show = (((color_mapshadow >> 24) & 0xFF) > 0);
}

void PmapRenderer::Initialize(IDirect3DDevice9* device) {

	GW::PathingMapArray path_map;
	if (GW::Map().IsMapLoaded()) {
		path_map = GW::Map().GetPathingMap();
	} else {
		initialized_ = false;
		return; // no map loaded yet, so don't render anything
	}

	// get the number of trapezoids, need it to allocate the vertex buffer
	trapez_count_ = 0;
	for (size_t i = 0; i < path_map.size(); ++i) {
		trapez_count_ += path_map[i].trapezoidcount;
	}
	if (trapez_count_ == 0) return;

	total_tri_count_ = tri_count_ = trapez_count_ * 2;
	if (shadow_show) total_tri_count_ = tri_count_ * 2;

	vert_count_ = tri_count_ * 3;
	total_vert_count_ = total_tri_count_ * 3;

	type_ = D3DPT_TRIANGLELIST;
	D3DVertex* vertices = nullptr;

	// allocate new vertex buffer
	if (buffer_) buffer_->Release();
	device->CreateVertexBuffer(sizeof(D3DVertex) * total_vert_count_, D3DUSAGE_WRITEONLY,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer_, NULL);
	buffer_->Lock(0, sizeof(D3DVertex) * total_vert_count_,
		(VOID**)&vertices, D3DLOCK_DISCARD);

	D3DVertex* vertices_begin = vertices;

	// populate vertex buffer
	for (int k = 0; k < (shadow_show ? 2 : 1); ++k) {
		for (size_t i = 0; i < path_map.size(); ++i) {
			GW::PathingMap pmap = path_map[i];
			for (size_t j = 0; j < pmap.trapezoidcount; ++j) {
				GW::PathingTrapezoid& trap = pmap.trapezoids[j];
				vertices[0].x = trap.XTL;
				vertices[0].y = trap.YT;
				vertices[1].x = trap.XTR;
				vertices[1].y = trap.YT;
				vertices[2].x = trap.XBL;
				vertices[2].y = trap.YB;
				vertices[3].x = trap.XBL;
				vertices[3].y = trap.YB;
				vertices[4].x = trap.XTR;
				vertices[4].y = trap.YT;
				vertices[5].x = trap.XBR;
				vertices[5].y = trap.YB;
				vertices += 6;
			}
		}
	}

	vertices = vertices_begin;
	for (unsigned int i = 0; i < total_vert_count_; ++i) {
		vertices[i].z = 0.0f;
		if (shadow_show && i < vert_count_) {
			vertices[i].color = color_mapshadow;
		} else {
			vertices[i].color = color_map;
		}
	}

	buffer_->Unlock();
}

void PmapRenderer::Render(IDirect3DDevice9* device) {
	if (!initialized_) {
		initialized_ = true;
		Initialize(device);
	}

	if (shadow_show) {
		D3DXMATRIX oldview, translate, newview;
		D3DXMatrixTranslation(&translate, 0, -100, 0.0f);
		device->GetTransform(D3DTS_VIEW, &oldview);
		newview = oldview * translate;
		device->SetTransform(D3DTS_VIEW, &newview);

		device->SetFVF(D3DFVF_CUSTOMVERTEX);
		device->SetStreamSource(0, buffer_, 0, sizeof(D3DVertex));
		device->DrawPrimitive(type_, 0, tri_count_);

		device->SetTransform(D3DTS_VIEW, &oldview);

		device->DrawPrimitive(type_, vert_count_ , tri_count_);
	} else {
		device->SetFVF(D3DFVF_CUSTOMVERTEX);
		device->SetStreamSource(0, buffer_, 0, sizeof(D3DVertex));
		device->DrawPrimitive(type_, 0, tri_count_);
	}
}
