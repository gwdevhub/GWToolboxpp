#include "stdafx.h"
#include "CustomRenderer.h"

#include <d3dx9math.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Hero.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/MapMgr.h>

#include <Modules/Resources.h>

Color CustomRenderer::color = 0xFF00FFFF;

#define IniFilename L"Markers.ini"

void CustomRenderer::LoadSettings(CSimpleIni* ini, const char* section) {
	color = Colors::Load(ini, section, "color_custom_markers", 0xFFFFFFFF);
	Invalidate();
	LoadMarkers();
}
void CustomRenderer::LoadMarkers() {
	// clear current markers
	lines.clear();
	markers.clear();

	if (inifile == nullptr) inifile = new CSimpleIni(false, false, false);
	inifile->LoadFile(Resources::GetPath(IniFilename).c_str());

	// then load new
	CSimpleIni::TNamesDepend entries;
	inifile->GetAllSections(entries);
	for (CSimpleIni::Entry& entry : entries) {
		const char* section = entry.pItem;
		if (strncmp(section, "customline", 10) == 0) {
			lines.push_back(CustomLine(inifile->GetValue(section, "name", "line")));
			lines.back().p1.x = (float)inifile->GetDoubleValue(section, "x1", 0.0);
			lines.back().p1.y = (float)inifile->GetDoubleValue(section, "y1", 0.0);
			lines.back().p2.x = (float)inifile->GetDoubleValue(section, "x2", 0.0);
			lines.back().p2.y = (float)inifile->GetDoubleValue(section, "y2", 0.0);
			lines.back().map = (GW::Constants::MapID)inifile->GetLongValue(section, "map", 0);
			lines.back().visible = inifile->GetBoolValue(section, "visible", true);
			inifile->Delete(section, nullptr);
		}
		if (strncmp(section, "custommarker", 12) == 0) {
			markers.push_back(CustomMarker(inifile->GetValue(section, "name", "marker")));
			markers.back().pos.x = (float)inifile->GetDoubleValue(section, "x", 0.0);
			markers.back().pos.y = (float)inifile->GetDoubleValue(section, "y", 0.0);
			markers.back().size = (float)inifile->GetDoubleValue(section, "size", 0.0);
			markers.back().shape = (Shape)inifile->GetLongValue(section, "shape", 0);
			markers.back().map = (GW::Constants::MapID)inifile->GetLongValue(section, "map", 0);
			markers.back().visible = inifile->GetBoolValue(section, "visible", true);
			inifile->Delete(section, nullptr);
		}
	}

	markers_changed = false;
}
void CustomRenderer::SaveSettings(CSimpleIni* ini, const char* section) const {
	Colors::Save(ini, section, "color_custom_markers", color);
	SaveMarkers();
}
void CustomRenderer::SaveMarkers() const {
	// clear markers from ini
	// then load new
	if (markers_changed) {
		CSimpleIni::TNamesDepend entries;
		inifile->GetAllSections(entries);
		for (CSimpleIni::Entry& entry : entries) {
			const char* section = entry.pItem;
			if (strncmp(section, "customline", 10) == 0) {
				inifile->Delete(section, nullptr);
			}
			if (strncmp(section, "custommarker", 12) == 0) {
				inifile->Delete(section, nullptr);
			}
		}

		// then save
		for (unsigned i = 0; i < lines.size(); ++i) {
			const CustomLine& line = lines[i];
			char section[32];
			snprintf(section, 32, "customline%03d", i);
			inifile->SetValue(section, "name", line.name);
			inifile->SetDoubleValue(section, "x1", line.p1.x);
			inifile->SetDoubleValue(section, "y1", line.p1.y);
			inifile->SetDoubleValue(section, "x2", line.p2.x);
			inifile->SetDoubleValue(section, "y2", line.p2.y);
			inifile->SetLongValue(section, "map", (long)line.map);
			inifile->SetBoolValue(section, "visible", line.visible);
		}
		for (unsigned i = 0; i < markers.size(); ++i) {
			const CustomMarker& marker = markers[i];
			char section[32];
			snprintf(section, 32, "custommarker%03d", i);
			inifile->SetValue(section, "name", marker.name);
			inifile->SetDoubleValue(section, "x", marker.pos.x);
			inifile->SetDoubleValue(section, "y", marker.pos.y);
			inifile->SetDoubleValue(section, "size", marker.size);
			inifile->SetLongValue(section, "shape", marker.shape);
			inifile->SetLongValue(section, "map", (long)marker.map);
			inifile->SetBoolValue(section, "visible", marker.visible);
		}

		inifile->SaveFile(Resources::GetPath(IniFilename).c_str());
	}
}
void CustomRenderer::Invalidate() {
	initialized = false;
	fullcircle.Invalidate();
	linecircle.Invalidate();
}
void CustomRenderer::DrawSettings() {
	if (ImGui::SmallButton("Restore Defaults")) {
		color = 0xFFFFFFFF;
		Invalidate();
	}
	if (Colors::DrawSettingHueWheel("Color", &color)) Invalidate();
	ImGui::Text("Note: custom markers are stored in 'Markers.ini' in settings folder. You can share the file with other players or paste other people's markers into it.");
	float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
	ImGui::PushID("lines");
	for (unsigned i = 0; i < lines.size(); ++i) {
		bool remove = false;
		CustomLine& line = lines[i];
		ImGui::PushID(i);
		if (ImGui::Checkbox("##visible", &line.visible)) markers_changed = true;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Visible");
		ImGui::SameLine(0.0f, spacing);
		ImGui::PushItemWidth((ImGui::CalcItemWidth() - ImGui::GetTextLineHeightWithSpacing() - spacing * 5) / 5);
		if (ImGui::DragFloat("##x1", &line.p1.x, 1.0f, 0.0f, 0.0f, "%.0f")) markers_changed = true;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Line X 1");
		ImGui::SameLine(0.0f, spacing);
		if (ImGui::DragFloat("##y1", &line.p1.y, 1.0f, 0.0f, 0.0f, "%.0f")) markers_changed = true;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Line Y 1");
		ImGui::SameLine(0.0f, spacing);
		if (ImGui::DragFloat("##x2", &line.p2.x, 1.0f, 0.0f, 0.0f, "%.0f")) markers_changed = true;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Line X 2");
		ImGui::SameLine(0.0f, spacing);
		if (ImGui::DragFloat("##y2", &line.p2.y, 1.0f, 0.0f, 0.0f, "%.0f")) markers_changed = true;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Line Y 2");
		ImGui::SameLine(0.0f, spacing);
		if (ImGui::InputInt("##map", (int*)&line.map, 0)) markers_changed = true;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Map ID");
		ImGui::SameLine(0.0f, spacing);
		ImGui::PopItemWidth();
		ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() - ImGui::GetCursorPosX() - spacing - 20.0f);
		if (ImGui::InputText("##name", line.name, 128)) markers_changed = true;
		ImGui::PopItemWidth();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Name");
		ImGui::SameLine(0.0f, spacing);
		if (ImGui::Button("x##delete", ImVec2(20.0f, 0))) {
			remove = true;
			markers_changed = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete");
		ImGui::PopID();
		if (remove) lines.erase(lines.begin() + i);
	}
	ImGui::PopID();
	ImGui::PushID("markers");
	for (unsigned i = 0; i < markers.size(); ++i) {
		bool remove = false;
		CustomMarker& marker = markers[i];
		ImGui::PushID(i);
		ImGui::Checkbox("##visible", &marker.visible);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Visible");
		ImGui::SameLine(0.0f, spacing);
		ImGui::PushItemWidth((ImGui::CalcItemWidth() - ImGui::GetTextLineHeightWithSpacing() - spacing * 5) / 5);
		if (ImGui::DragFloat("##x", &marker.pos.x, 1.0f, 0.0f, 0.0f, "%.0f")) markers_changed = true;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Marker X Position");
		ImGui::SameLine(0.0f, spacing);
		if (ImGui::DragFloat("##y", &marker.pos.y, 1.0f, 0.0f, 0.0f, "%.0f")) markers_changed = true;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Marker Y Position");
		ImGui::SameLine(0.0f, spacing);
		if (ImGui::DragFloat("##size", &marker.size, 1.0f, 0.0f, 0.0f, "%.0f")) markers_changed = true;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Size");
		ImGui::SameLine(0.0f, spacing);

		static const char* const types[] = {
			"Circle",
			"FillCircle"
		};
		if (ImGui::Combo("##type", (int*)&marker.shape, types, 2)) markers_changed = true;
		ImGui::SameLine(0.0f, spacing);

		if (ImGui::InputInt("##map", (int*)&marker.map, 0)) markers_changed = true;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Map ID");
		ImGui::SameLine(0.0f, spacing);
		ImGui::PopItemWidth();
		ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() - ImGui::GetCursorPosX() - spacing - 20.0f);
		if (ImGui::InputText("##name", marker.name, 128)) markers_changed = true;
		ImGui::PopItemWidth();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Name");
		ImGui::SameLine(0.0f, spacing);
		if (ImGui::Button("x##delete", ImVec2(20.0f, 0))) {
			remove = true;
			markers_changed = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete");
		ImGui::PopID();
		if (remove) markers.erase(markers.begin() + i);
	}
	ImGui::PopID();
	float button_width = (ImGui::CalcItemWidth() - ImGui::GetStyle().ItemSpacing.x) / 2;
	if (ImGui::Button("Add Line", ImVec2(button_width, 0.0f))) {
		char buf[32];
		snprintf(buf, 32, "line%d", lines.size());
		lines.push_back(CustomLine(buf));
		markers_changed = true;
	}
	ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
	if (ImGui::Button("Add Marker", ImVec2(button_width, 0.0f))) {
		char buf[32];
		snprintf(buf, 32, "marker%d", markers.size());
		markers.push_back(CustomMarker(buf));
		markers_changed = true;
	}
}

void CustomRenderer::Initialize(IDirect3DDevice9* device) {
	type = D3DPT_LINELIST;
	vertices_max = 0x100; // support for up to 256 line segments, should be enough
	vertices = nullptr;

	HRESULT hr = device->CreateVertexBuffer(sizeof(D3DVertex) * vertices_max, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
	if (FAILED(hr)) {
		printf("Error setting up CustomRenderer vertex buffer: %d\n", hr);
	}
}

void CustomRenderer::FullCircle::Initialize(IDirect3DDevice9* device) {
	type = D3DPT_TRIANGLEFAN;
	count = 48; // polycount
	unsigned int vertex_count = count + 2;
	D3DVertex* vertices = nullptr;

	if (buffer) buffer->Release();
	device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
	buffer->Lock(0, sizeof(D3DVertex) * vertex_count,
		(VOID**)&vertices, D3DLOCK_DISCARD);

	const float PI = 3.1415927f;
	vertices[0].x = 0.0f;
	vertices[0].y = 0.0f;
	vertices[0].z = 0.0f;
	vertices[0].color = Colors::Sub(color, Colors::ARGB(50, 0, 0, 0));
	for (size_t i = 1; i < vertex_count; ++i) {
		float angle = (i - 1) * (2 * PI / count);
		vertices[i].x = std::cos(angle);
		vertices[i].y = std::sin(angle);
		vertices[i].z = 0.0f;
		vertices[i].color = color;
	}

	buffer->Unlock();
}

void CustomRenderer::LineCircle::Initialize(IDirect3DDevice9* device) {
	type = D3DPT_LINESTRIP;
	count = 48; // polycount
	unsigned int vertex_count = count + 1;
	D3DVertex* vertices = nullptr;

	if (buffer) buffer->Release();
	device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, 0, 
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
	buffer->Lock(0, sizeof(D3DVertex) * vertex_count, (VOID**)&vertices, D3DLOCK_DISCARD);

	for (size_t i = 0; i < count; ++i) {
		float angle = i * (2 * static_cast<float>(M_PI) / (count + 1));
		vertices[i].x = std::cos(angle);
		vertices[i].y = std::sin(angle);
		vertices[i].z = 0.0f;
		vertices[i].color = color; // 0xFF666677;
	}
	vertices[count] = vertices[0];

	buffer->Unlock();
}

void CustomRenderer::Render(IDirect3DDevice9* device) {
	if (!initialized) {
		initialized = true;
		Initialize(device);
	}

	DrawCustomMarkers(device);

	vertices_count = 0;
	HRESULT res = buffer->Lock(0, sizeof(D3DVertex) * vertices_max, (VOID**)&vertices, D3DLOCK_DISCARD);
	if (FAILED(res)) printf("PingsLinesRenderer Lock() error: %d\n", res);

	DrawCustomLines(device);

	D3DXMATRIX i;
	D3DXMatrixIdentity(&i);
	device->SetTransform(D3DTS_WORLD, &i);

	buffer->Unlock();
	if (vertices_count != 0) {
		device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));
		device->DrawPrimitive(type, 0, vertices_count / 2);
		vertices_count = 0;
	}
}

void CustomRenderer::DrawCustomMarkers(IDirect3DDevice9* device) {
	if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
		for (const CustomMarker& marker : markers) {
			if (marker.visible 
				&& (marker.map == GW::Constants::MapID::None || marker.map == GW::Map::GetMapID())) {
				D3DXMATRIX translate, scale, world;
				D3DXMatrixTranslation(&translate, marker.pos.x, marker.pos.y, 0.0f);
				D3DXMatrixScaling(&scale, marker.size, marker.size, 1.0f);
				world = scale * translate;
				device->SetTransform(D3DTS_WORLD, &world);

				switch (marker.shape) {
				case LineCircle: linecircle.Render(device); break;
				case FullCircle: fullcircle.Render(device); break;
				}
			}
		}

		GW::HeroFlagArray& flags = GW::GameContext::instance()->world->hero_flags;
		if (flags.valid()) {
			for (unsigned i = 0; i < flags.size(); ++i) {
				GW::HeroFlag& flag = flags[i];
				D3DXMATRIX translate, scale, world;
				D3DXMatrixTranslation(&translate, flag.flag.x, flag.flag.y, 0.0f);
				D3DXMatrixScaling(&scale, 200.0f, 200.0f, 1.0f);
				world = scale * translate;
				device->SetTransform(D3DTS_WORLD, &world);
				linecircle.Render(device);
			}
		}
		GW::Vec3f allflag = GW::GameContext::instance()->world->all_flag;
		D3DXMATRIX translate, scale, world;
		D3DXMatrixTranslation(&translate, allflag.x, allflag.y, 0.0f);
		D3DXMatrixScaling(&scale, 300.0f, 300.0f, 1.0f);
		world = scale * translate;
		device->SetTransform(D3DTS_WORLD, &world);
		linecircle.Render(device);
	}
}

void CustomRenderer::DrawCustomLines(IDirect3DDevice9* device) {
	if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
		for (const CustomLine& line : lines) {
			if (line.visible 
				&& (line.map == GW::Constants::MapID::None || line.map == GW::Map::GetMapID())) {
				EnqueueVertex(line.p1.x, line.p1.y, color);
				EnqueueVertex(line.p2.x, line.p2.y, color);
			}
		}
	}
}

void CustomRenderer::EnqueueVertex(float x, float y, Color color) {
	if (vertices_count == vertices_max) return;
	vertices[0].x = x;
	vertices[0].y = y;
	vertices[0].z = 0.0f;
	vertices[0].color = color;
	++vertices;
	++vertices_count;
}
