#pragma once

#include "Viewer.h"
#include "VBuffer.h"
#include "PmapRenderer.h"
#include "AgentRenderer.h"
#include "RangeRenderer.h"
#include "PingsLinesRenderer.h"

class Minimap : public Viewer {
public:
	Minimap();

	void Render(IDirect3DDevice9* device) override;

	bool OnMouseDown(MSG msg);
	bool OnMouseUp(MSG msg);
	bool OnMouseMove(MSG msg);
	bool OnMouseWheel(MSG msg);

private:
	bool dragging_;
	int drag_start_x_;
	int drag_start_y_;

	RangeRenderer range_renderer;
	PmapRenderer pmap_renderer;
	AgentRenderer agent_renderer;
	PingsLinesRenderer pingslines_renderer;
};
