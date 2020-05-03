#pragma once

#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/GamePos.h>
#include "VBuffer.h"
#include <Color.h>

class CustomRenderer : public VBuffer {
	struct CustomLine {
		CustomLine(float x1, float y1, float x2, float y2, GW::Constants::MapID m, const char* n)
			: p1(x1, y1), p2(x2, y2), map(m), visible(true) {
			if (n) GuiUtils::StrCopy(name, n, sizeof(name));
			else GuiUtils::StrCopy(name, "line", sizeof(name));
		};
		CustomLine(const char* n) : CustomLine(0, 0, 0, 0, GW::Constants::MapID::None, n) {};
		GW::Vec2f p1;
		GW::Vec2f p2;
		GW::Constants::MapID map;
		bool visible;
		char name[128];
	};
	enum Shape {
		LineCircle,
		FullCircle
	};
	struct CustomMarker {
		CustomMarker(float x, float y, float s, Shape sh, GW::Constants::MapID m, const char* n)
			: pos(x, y), size(s), shape(sh), map(m), visible(true) {
			if (n) GuiUtils::StrCopy(name, n, sizeof(name));
			else GuiUtils::StrCopy(name, "marker", sizeof(name));
		};
		CustomMarker(const char* n) : CustomMarker(0, 0, 100.0f, LineCircle, GW::Constants::MapID::None, n) {};
		GW::Vec2f pos;
		float size;
		Shape shape;
		GW::Constants::MapID map;
		bool visible;
		char name[128];
	};

public:
	void Render(IDirect3DDevice9* device) override;

	void DrawSettings();
	void LoadSettings(CSimpleIni* ini, const char* section);
	void SaveSettings(CSimpleIni* ini, const char* section) const;
	void LoadMarkers();
	void SaveMarkers() const;

private:
	void Invalidate() override;
	void Initialize(IDirect3DDevice9* device) override;
	void DrawCustomMarkers(IDirect3DDevice9* device);
	void DrawCustomLines(IDirect3DDevice9* device);
	void EnqueueVertex(float x, float y, Color color);

	class FullCircle : public VBuffer {
		void Initialize(IDirect3DDevice9* device) override;
	} fullcircle;
	class LineCircle : public VBuffer {
		void Initialize(IDirect3DDevice9* device) override;
	} linecircle;
	
	static Color color;

	D3DVertex* vertices = nullptr;
	unsigned int vertices_count = 0;
	unsigned int vertices_max = 0;

	bool markers_changed = false;
	std::vector<CustomLine> lines;
	std::vector<CustomMarker> markers;

	CSimpleIni* inifile = nullptr;
};
