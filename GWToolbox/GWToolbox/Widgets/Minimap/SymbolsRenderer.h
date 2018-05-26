#pragma once

#include <vector>

#include <GWCA\GameEntities\Position.h>

#include "VBuffer.h"
#include "Color.h"

class SymbolsRenderer : public VBuffer {
public:
	SymbolsRenderer() {}

	void Render(IDirect3DDevice9* device) override;

	void DrawSettings();
	void LoadSettings(CSimpleIni* ini, const char* section);
	void SaveSettings(CSimpleIni* ini, const char* section) const;

private:

	void Initialize(IDirect3DDevice9* device) override;

	Color color_quest;
	Color color_north;
	Color color_modifier;

	const DWORD star_ntriangles = 16;
	DWORD star_offset;

	const DWORD arrow_ntriangles = 2;
	DWORD arrow_offset;
	
	const DWORD north_ntriangles = 2;
	DWORD north_offset;
};
