#include "PmapRenderer.h"

#include <vector>

PmapRenderer::PmapRenderer()
	: pmap(PathingMap(0)) {
}

void PmapRenderer::Initialize(IDirect3DDevice9* device) {

	std::vector<PathingMapTrapezoid>trapez = pmap.GetPathingData();
	short max_plane = 0;
	for (size_t i = 0; i < trapez.size(); ++i) {
		if (max_plane < trapez[i].Plane) {
			max_plane = trapez[i].Plane;
		}
	}

	count_ = trapez.size() * 2;
	type_ = D3DPT_TRIANGLELIST;
	Vertex* vertices = nullptr;

	device->CreateVertexBuffer(sizeof(Vertex) * count_ * 3, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer_, NULL);
	buffer_->Lock(0, sizeof(Vertex) * count_ * 3,
		(VOID**)&vertices, D3DLOCK_DISCARD);

	for (size_t i = 0; i < trapez.size(); ++i) {
		DWORD c = trapez[i].Plane * 255 / max_plane / 2;
		DWORD color = D3DCOLOR_ARGB(0xAA, 0xFF - c, 0xFF - c, 0xFF);
		for (int j = 0; j < 6; ++j) {
			vertices[i * 6 + j].z = 1.0f;
			vertices[i * 6 + j].color = color;
		}
		// triangle 1
		// topleft
		vertices[i * 6 + 0].x = trapez[i].XTL;
		vertices[i * 6 + 0].y = trapez[i].YT;

		// topright
		vertices[i * 6 + 1].x = trapez[i].XTR;
		vertices[i * 6 + 1].y = trapez[i].YT;

		// botleft
		vertices[i * 6 + 2].x = trapez[i].XBL;
		vertices[i * 6 + 2].y = trapez[i].YB;

		// triangle 2
		// botleft
		vertices[i * 6 + 3].x = trapez[i].XBL;
		vertices[i * 6 + 3].y = trapez[i].YB;

		// topright
		vertices[i * 6 + 4].x = trapez[i].XTR;
		vertices[i * 6 + 4].y = trapez[i].YT;

		// botright
		vertices[i * 6 + 5].x = trapez[i].XBR;
		vertices[i * 6 + 5].y = trapez[i].YB;
	}

	buffer_->Unlock();
}

void PmapRenderer::LoadMap(unsigned long file_hash) {
	printf("Loading map\n");
	pmap = PathingMap(file_hash);

	wchar_t filename[MAX_PATH];
	wsprintf(filename, L"PMAPs\\MAP %010u.pmap", file_hash);
	bool loaded = pmap.Open(filename);
	if (loaded) {
		printf("loaded pmap!\n");
	} else {
		printf("failed to load pmap!\n");
	}
	printf("number of trapzeoids %d\n", pmap.GetPathingData().size());
}