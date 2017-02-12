#pragma once

#include "Viewer.h"
#include "VBuffer.h"
#include "PmapRenderer.h"
#include "AgentRenderer.h"
#include "RangeRenderer.h"
#include "PingsLinesRenderer.h"
#include "SymbolsRenderer.h"

class Minimap : public Viewer {
public:
	Minimap();

	inline static const char* IniSection() { return "minimap"; }
	inline static const char* IniKeyX() { return "x"; }
	inline static const char* IniKeyY() { return "y"; }
	inline static const char* IniKeySize() { return "size"; }
	inline static const char* IniKeyScale() { return "scale"; }
	inline static const char* InikeyShow() { return "show"; }

	void Render(IDirect3DDevice9* device) override;

	void SetFreeze(bool b) { freeze_ = b; }
	void SetVisible(bool v) { 
		visible_ = v;
		pingslines_renderer.SetVisible(v);
	}

	bool OnMouseDown(MSG msg);
	bool OnMouseDblClick(MSG msg);
	bool OnMouseUp(MSG msg);
	bool OnMouseMove(MSG msg);
	bool OnMouseWheel(MSG msg);

private:
	bool IsInside(int x, int y) const;
	// returns true if the map is visible, valid, not loading, etc
	inline bool IsActive() const;

	GW::Vector2f InterfaceToWorldPoint(int x, int y) const;
	GW::Vector2f InterfaceToWorldVector(int x, int y) const;
	void SelectTarget(GW::Vector2f pos);

	bool freeze_;
	bool mousedown_;
	bool visible_;

	int drag_start_x_;
	int drag_start_y_;

	// vars for minimap movement
	clock_t last_moved_;

	bool loading_; // only consider some cases but still good

	RangeRenderer range_renderer;
	PmapRenderer pmap_renderer;
	AgentRenderer agent_renderer;
	PingsLinesRenderer pingslines_renderer;
	SymbolsRenderer symbols_renderer;
};
