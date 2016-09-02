#pragma once

#include <vector>

#include <GWCA\GameEntities\Position.h>

#include "VBuffer.h"
#include "MinimapUtils.h"

class SymbolsRenderer : public VBuffer {
public:
	SymbolsRenderer();

	void Render(IDirect3DDevice9* device) override;

private:

	void Initialize(IDirect3DDevice9* device) override;

	MinimapUtils::Color color_quest;
	MinimapUtils::Color color_north;

	const DWORD star_ntriangles = 16;
	DWORD star_offset;

	const DWORD arrow_ntriangles = 2;
	DWORD arrow_offset;
	
	const DWORD north_ntriangles = 2;
	DWORD north_offset;
};
