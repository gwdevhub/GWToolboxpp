#include "CustomRenderer.h"

#include <d3dx9math.h>
#include <GWCA\GWCA.h>
#include <GWCA\Managers\MapMgr.h>

Color CustomRenderer::color = 0xFF00FFFF;

void CustomRenderer::LoadSettings(CSimpleIni* ini, const char* section) {
	color = Colors::Load(ini, section, "color_custom_markers", 0xFFFFFFFF);
	Invalidate();

	// clear current markers
	lines.clear();
	markers.clear();

	// then load new
	CSimpleIni::TNamesDepend entries;
	ini->GetAllSections(entries);
	for (CSimpleIni::Entry& entry : entries) {
		const char* section = entry.pItem;
		if (strncmp(section, "customline", 10) == 0) {
			lines.push_back(CustomLine(ini->GetValue(section, "name", "line")));
			lines.back().p1.x = (float)ini->GetDoubleValue(section, "x1", 0.0);
			lines.back().p1.y = (float)ini->GetDoubleValue(section, "y1", 0.0);
			lines.back().p2.x = (float)ini->GetDoubleValue(section, "x2", 0.0);
			lines.back().p2.y = (float)ini->GetDoubleValue(section, "y2", 0.0);
			lines.back().map = (GW::Constants::MapID)ini->GetLongValue(section, "map", 0);
			ini->Delete(section, nullptr);
		}
		if (strncmp(section, "custommarker", 12) == 0) {
			markers.push_back(CustomMarker(ini->GetValue(section, "name", "marker")));
			markers.back().pos.x = (float)ini->GetDoubleValue(section, "x", 0.0);
			markers.back().pos.y = (float)ini->GetDoubleValue(section, "y", 0.0);
			markers.back().size = (float)ini->GetDoubleValue(section, "size", 0.0);
			markers.back().shape = (Shape)ini->GetLongValue(section, "shape", 0);
			markers.back().map = (GW::Constants::MapID)ini->GetLongValue(section, "map", 0);
			ini->Delete(section, nullptr);
		}
	}
}
void CustomRenderer::SaveSettings(CSimpleIni* ini, const char* section) const {
	Colors::Save(ini, section, "color_custom_markers", color);

	// clear markers from ini
	// then load new
	CSimpleIni::TNamesDepend entries;
	ini->GetAllSections(entries);
	for (CSimpleIni::Entry& entry : entries) {
		const char* section = entry.pItem;
		if (strncmp(section, "customline", 10) == 0) {
			ini->Delete(section, nullptr);
		}
		if (strncmp(section, "custommarker", 12) == 0) {
			ini->Delete(section, nullptr);
		}
	}

	// then save
	for (unsigned i = 0; i < lines.size(); ++i) {
		const CustomLine& line = lines[i];
		char section[32];
		sprintf_s(section, "customline%03d", i);
		ini->SetValue(section, "name", line.name);
		ini->SetDoubleValue(section, "x1", line.p1.x);
		ini->SetDoubleValue(section, "y1", line.p1.y);
		ini->SetDoubleValue(section, "x2", line.p2.x);
		ini->SetDoubleValue(section, "y2", line.p2.y);
		ini->SetLongValue(section, "map", (long)line.map);
	}
	for (unsigned i = 0; i < markers.size(); ++i) {
		const CustomMarker& marker = markers[i];
		char section[32];
		sprintf_s(section, "custommarker%03d", i);
		ini->SetValue(section, "name", marker.name);
		ini->SetDoubleValue(section, "x", marker.pos.x);
		ini->SetDoubleValue(section, "y", marker.pos.y);
		ini->SetDoubleValue(section, "size", marker.size);
		ini->SetLongValue(section, "shape", marker.shape);
		ini->SetLongValue(section, "map", (long)marker.map);
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
	if (Colors::DrawSetting("Color", &color)) Invalidate();
	float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
	ImGui::PushID("lines");
	for (unsigned i = 0; i < lines.size(); ++i) {
		bool remove = false;
		CustomLine& line = lines[i];
		ImGui::PushID(i);
		ImGui::PushItemWidth((ImGui::CalcItemWidth() - spacing * 4) / 5);
		ImGui::DragFloat("##x1", &line.p1.x, 1.0f, 0.0f, 0.0f, "%.0f");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Line X 1");
		ImGui::SameLine(0.0f, spacing);
		ImGui::DragFloat("##y1", &line.p1.y, 1.0f, 0.0f, 0.0f, "%.0f");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Line Y 1");
		ImGui::SameLine(0.0f, spacing);
		ImGui::DragFloat("##x2", &line.p2.x, 1.0f, 0.0f, 0.0f, "%.0f");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Line X 2");
		ImGui::SameLine(0.0f, spacing);
		ImGui::DragFloat("##y2", &line.p2.y, 1.0f, 0.0f, 0.0f, "%.0f");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Line Y 2");
		ImGui::SameLine(0.0f, spacing);
		ImGui::InputInt("##map", (int*)&line.map, 0);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Map ID");
		ImGui::SameLine(0.0f, spacing);
		ImGui::PopItemWidth();
		ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() - ImGui::GetCursorPosX() - spacing - 20.0f);
		ImGui::InputText("##name", line.name, 128);
		ImGui::PopItemWidth();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Name");
		ImGui::SameLine(0.0f, spacing);
		if (ImGui::Button("x##delete", ImVec2(20.0f, 0))) {
			remove = true;
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
		ImGui::PushItemWidth((ImGui::CalcItemWidth() - spacing * 4) / 5);
		ImGui::DragFloat("##x", &marker.pos.x, 1.0f, 0.0f, 0.0f, "%.0f");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Marker X Position");
		ImGui::SameLine(0.0f, spacing);
		ImGui::DragFloat("##y", &marker.pos.y, 1.0f, 0.0f, 0.0f, "%.0f");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Marker Y Position");
		ImGui::SameLine(0.0f, spacing);
		ImGui::DragFloat("##size", &marker.size, 1.0f, 0.0f, 0.0f, "%.0f");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Size");
		ImGui::SameLine(0.0f, spacing);

		static const char* const types[] = {
			"Circle",
			"FillCircle"
		};
		ImGui::Combo("##type", (int*)&marker.shape, types, 2);
		ImGui::SameLine(0.0f, spacing);

		ImGui::InputInt("##map", (int*)&marker.map, 0);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Map ID");
		ImGui::SameLine(0.0f, spacing);
		ImGui::PopItemWidth();
		ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() - ImGui::GetCursorPosX() - spacing - 20.0f);
		ImGui::InputText("##name", marker.name, 128);
		ImGui::PopItemWidth();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Name");
		ImGui::SameLine(0.0f, spacing);
		if (ImGui::Button("x##delete", ImVec2(20.0f, 0))) {
			remove = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete");
		ImGui::PopID();
		if (remove) markers.erase(markers.begin() + i);
	}
	ImGui::PopID();
	float button_width = (ImGui::CalcItemWidth() - ImGui::GetStyle().ItemSpacing.x) / 2;
	if (ImGui::Button("Add Line", ImVec2(button_width, 0.0f))) {
		char buf[32];
		sprintf_s(buf, "line%d", lines.size());
		lines.push_back(CustomLine(buf));
	}
	ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
	if (ImGui::Button("Add Marker", ImVec2(button_width, 0.0f))) {
		char buf[32];
		sprintf_s(buf, "marker%d", markers.size());
		markers.push_back(CustomMarker(buf));
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
			if (marker.map == GW::Constants::MapID::None || marker.map == GW::Map::GetMapID()) {
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
				D3DXMatrixScaling(&scale, 250.0f, 250.0f, 1.0f);
				world = scale * translate;
				device->SetTransform(D3DTS_WORLD, &world);
				linecircle.Render(device);
			}
		}
		GW::GamePos allflag = GW::GameContext::instance()->world->all_flag;
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
			if (line.map == GW::Constants::MapID::None || line.map == GW::Map::GetMapID()) {
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
