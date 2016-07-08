#pragma once

#include "Viewer.h"
#include "VBuffer.h"
#include "PmapRenderer.h"
#include "AgentRenderer.h"
#include "RangeRenderer.h"

class Minimap : public Viewer {
	class UIRenderer : public VBuffer {
		void Initialize(IDirect3DDevice9* device) override;
	};

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

	UIRenderer ui_renderer;
	RangeRenderer range_renderer;
	PmapRenderer pmap_renderer;
	AgentRenderer agent_renderer;
};
