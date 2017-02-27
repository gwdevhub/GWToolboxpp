#pragma once

#include "ToolboxWindow.h"
#include "VBuffer.h"
#include "PmapRenderer.h"
#include "AgentRenderer.h"
#include "RangeRenderer.h"
#include "PingsLinesRenderer.h"
#include "SymbolsRenderer.h"

class Minimap : public ToolboxWindow {
	struct Vec2i {
		Vec2i(int _x, int _y) : x(_x), y(_y) {}
		Vec2i() : x(0), y(0) {}
		int x, y;
	};
public:
	const int ms_before_back = 1000; // time before we snap back to player
	const float acceleration = 0.5f;
	const float max_speed = 15.0f; // game units per frame

	const char* Name() const override { return "Minimap"; }
	Minimap();
	~Minimap() {};

	void Draw(IDirect3DDevice9* device) override;
	void RenderSetupProjection(IDirect3DDevice9* device);

	bool OnMouseDown(UINT Message, WPARAM wParam, LPARAM lParam);
	bool OnMouseDblClick(UINT Message, WPARAM wParam, LPARAM lParam);
	bool OnMouseUp(UINT Message, WPARAM wParam, LPARAM lParam);
	bool OnMouseMove(UINT Message, WPARAM wParam, LPARAM lParam);
	bool OnMouseWheel(UINT Message, WPARAM wParam, LPARAM lParam);
	bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

	void LoadSettingInternal(CSimpleIni* ini) override;
	void SaveSettingInternal(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

	DWORD mapfile;

private:
	bool IsInside(int x, int y) const;
	// returns true if the map is visible, valid, not loading, etc
	inline bool IsActive() const;

	GW::Vector2f InterfaceToWorldPoint(Vec2i pos) const;
	GW::Vector2f InterfaceToWorldVector(Vec2i pos) const;
	void SelectTarget(GW::Vector2f pos);

	bool mousedown;

	Vec2i location;
	int size;

	Vec2i drag_start;
	GW::Vector2f translation;
	float scale;

	// vars for minimap movement
	clock_t last_moved;

	bool loading; // only consider some cases but still good

	RangeRenderer range_renderer;
	PmapRenderer pmap_renderer;
	AgentRenderer agent_renderer;
	PingsLinesRenderer pingslines_renderer;
	SymbolsRenderer symbols_renderer;
};
