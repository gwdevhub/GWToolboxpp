#pragma once

#include "Viewer.h"
#include "Renderer.h"
#include "PmapRenderer.h"

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

private:
	UIRenderer ui_renderer;
	RangeRenderer range_renderer;
	PmapRenderer pmap_renderer;

	void RenderAgents(IDirect3DDevice9* device);
};
