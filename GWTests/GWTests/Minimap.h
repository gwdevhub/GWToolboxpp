#pragma once

#include "Viewer.h"
#include "Renderer.h"
#include "PmapRenderer.h"
#include "AgentRenderer.h"

class Minimap : public Viewer {
	class UIRenderer : public Renderer {
		void Initialize(IDirect3DDevice9* device) override;
	};

	class RangeRenderer : public Renderer {
		static const int circle_points = 48;
		static const int circle_vertices = 49;
		void CreateCircle(Vertex* vertices, float radius);
		void Initialize(IDirect3DDevice9* device) override;
	public:
		void Render(IDirect3DDevice9* device) override;
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
	Point2i drag_start_;

	UIRenderer ui_renderer;
	RangeRenderer range_renderer;
	PmapRenderer pmap_renderer;
	AgentRenderer agent_renderer;
};
