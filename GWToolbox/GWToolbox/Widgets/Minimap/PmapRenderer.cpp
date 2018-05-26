#include "PmapRenderer.h"

#include <vector>
#include <d3dx9math.h>
#include <d3d9.h>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\CameraMgr.h>

#include "D3DVertex.h"

void PmapRenderer::LoadSettings(CSimpleIni* ini, const char* section) {
	color_map = Colors::Load(ini, section, "color_map", 0xFF999999);
	color_mapshadow = Colors::Load(ini, section, "color_mapshadow", 0xFF120808);
	Invalidate();
}

void PmapRenderer::SaveSettings(CSimpleIni* ini, const char* section) const {
	Colors::Save(ini, section, "color_map", color_map);
	Colors::Save(ini, section, "color_mapshadow", color_mapshadow);
}

void PmapRenderer::DrawSettings() {
	if (ImGui::SmallButton("Restore Defaults")) {
		color_map = 0xFF999999;
		color_mapshadow = 0xFF120808;
		Invalidate();
	}
	if (Colors::DrawSetting("Map", &color_map)) Invalidate();
	if (Colors::DrawSetting("Shadow", &color_mapshadow)) Invalidate();
}

void PmapRenderer::Initialize(IDirect3DDevice9* device) {

// #define WIREFRAME_MODE

	GW::PathingMapArray path_map;
	if (GW::Map::IsMapLoaded()) {
		path_map = GW::Map::GetPathingMap();
	} else {
		initialized = false;
		return; // no map loaded yet, so don't render anything
	}
	bool shadow_show = ((color_mapshadow & IM_COL32_A_MASK) > 0);

	// get the number of trapezoids, need it to allocate the vertex buffer
	trapez_count_ = 0;
	for (size_t i = 0; i < path_map.size(); ++i) {
		trapez_count_ += path_map[i].trapezoidcount;
	}
	if (trapez_count_ == 0) return;

#ifdef WIREFRAME_MODE

	total_tri_count_ = tri_count_ = trapez_count_ * 4;
	if (shadow_show) total_tri_count_ = tri_count_ * 2;

	vert_count_ = tri_count_ * 2;
	total_vert_count_ = total_tri_count_ * 2;

#else

	total_tri_count_ = tri_count_ = trapez_count_ * 2;
	if (shadow_show) total_tri_count_ = tri_count_ * 2;

	vert_count_ = tri_count_ * 3;
	total_vert_count_ = total_tri_count_ * 3;

#endif
	
	D3DVertex* vertices = nullptr;

	// allocate new vertex buffer
	if (buffer) buffer->Release();
	device->CreateVertexBuffer(sizeof(D3DVertex) * total_vert_count_, D3DUSAGE_WRITEONLY,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
	buffer->Lock(0, sizeof(D3DVertex) * total_vert_count_,
		(VOID**)&vertices, D3DLOCK_DISCARD);

	D3DVertex* vertices_begin = vertices;



#ifdef WIREFRAME_MODE
	type = D3DPT_LINELIST;

	// populate vertex buffer
	for (int k = 0; k < (shadow_show ? 2 : 1); ++k) {
		for (size_t i = 0; i < path_map.size(); ++i) {
			GW::PathingMap pmap = path_map[i];
			for (size_t j = 0; j < pmap.trapezoidcount; ++j) {
				GW::PathingTrapezoid& trap = pmap.trapezoids[j];
				vertices[0].x = trap.XTL; vertices[0].y = trap.YT;
				vertices[1].x = trap.XTR; vertices[1].y = trap.YT;

				vertices[2].x = trap.XTR; vertices[2].y = trap.YT;
				vertices[3].x = trap.XBR; vertices[3].y = trap.YB;

				vertices[4].x = trap.XBR; vertices[4].y = trap.YB;
				vertices[5].x = trap.XBL; vertices[5].y = trap.YB;

				vertices[6].x = trap.XBL; vertices[6].y = trap.YB;
				vertices[7].x = trap.XTL; vertices[7].y = trap.YT;

				vertices += 8;
			}
		}
	}
#else
	type = D3DPT_TRIANGLELIST;

	// populate vertex buffer
	for (int k = 0; k < (shadow_show ? 2 : 1); ++k) {
		for (size_t i = 0; i < path_map.size(); ++i) {
			GW::PathingMap pmap = path_map[i];
			for (size_t j = 0; j < pmap.trapezoidcount; ++j) {
				GW::PathingTrapezoid& trap = pmap.trapezoids[j];

				vertices[0].x = trap.XTL; vertices[0].y = trap.YT;
				vertices[1].x = trap.XTR; vertices[1].y = trap.YT;
				vertices[2].x = trap.XBL; vertices[2].y = trap.YB;

				vertices[3].x = trap.XBL; vertices[3].y = trap.YB;
				vertices[4].x = trap.XTR; vertices[4].y = trap.YT;
				vertices[5].x = trap.XBR; vertices[5].y = trap.YB;
				vertices += 6;
			}
		}
	}
#endif

	vertices = vertices_begin;
	for (unsigned int i = 0; i < total_vert_count_; ++i) {
		vertices[i].z = 0.0f;
		if (shadow_show && i < vert_count_) {
			vertices[i].color = color_mapshadow;
		} else {
			vertices[i].color = color_map;
		}
	}

	buffer->Unlock();
}

void PmapRenderer::Render(IDirect3DDevice9* device) {
	if (!initialized) {
		initialized = true;
		Initialize(device);
	}

	if ((color_mapshadow & IM_COL32_A_MASK) > 0) {
		D3DXMATRIX oldview, translate, newview;
		D3DXMatrixTranslation(&translate, 0, -100, 0.0f);
		device->GetTransform(D3DTS_VIEW, &oldview);
		newview = oldview * translate;
		device->SetTransform(D3DTS_VIEW, &newview);

		device->SetFVF(D3DFVF_CUSTOMVERTEX);
		device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));
		device->DrawPrimitive(type, 0, tri_count_);

		device->SetTransform(D3DTS_VIEW, &oldview);

		device->DrawPrimitive(type, vert_count_ , tri_count_);
	} else {
		device->SetFVF(D3DFVF_CUSTOMVERTEX);
		device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));
		device->DrawPrimitive(type, 0, tri_count_);
	}
}
