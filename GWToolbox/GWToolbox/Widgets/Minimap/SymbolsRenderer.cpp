#include "SymbolsRenderer.h"

#include <ImGuiAddons.h>
#include <d3dx9math.h>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Context\GameContext.h>
#include <GWCA\Context\WorldContext.h>
#include "Minimap.h"

void SymbolsRenderer::LoadSettings(CSimpleIni* ini, const char* section) {
	color_quest = Colors::Load(ini, section, "color_quest", 0xFF22EF22);
	color_north = Colors::Load(ini, section, "color_north", 0xFFFF8000);
	color_modifier = Colors::Load(ini, section, "color_symbols_modifier", 0x001E1E1E);
	Invalidate();
}
void SymbolsRenderer::SaveSettings(CSimpleIni* ini, const char* section) const {
	Colors::Save(ini, section, "color_quest", color_quest);
	Colors::Save(ini, section, "color_north", color_north);
	Colors::Save(ini, section, "color_symbols_modifier", color_modifier);
}
void SymbolsRenderer::DrawSettings() {
	if (ImGui::SmallButton("Restore Defaults")) {
		color_quest = 0xFF22EF22;
		color_north = 0xFFFF8000;
		color_modifier = 0x001E1E1E;
		Invalidate();
	}
	if (Colors::DrawSetting("Quest Marker", &color_quest)) Invalidate();
	if (Colors::DrawSetting("North Marker", &color_north)) Invalidate();
	if (Colors::DrawSetting("Symbol Modifier", &color_modifier)) Invalidate();
	ImGui::ShowHelp("Each symbol has this value removed on the border and added at the center\nZero makes them have solid color, while a high number makes them appear more shaded.");
}

void SymbolsRenderer::Initialize(IDirect3DDevice9* device) {
	type = D3DPT_TRIANGLELIST;

	D3DVertex* vertices = nullptr;
	DWORD vertex_count = (star_ntriangles + arrow_ntriangles + north_ntriangles) * 3;
	DWORD offset = 0;

	device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, D3DUSAGE_WRITEONLY,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
	buffer->Lock(0, sizeof(D3DVertex) * vertex_count,
		(VOID**)&vertices, D3DLOCK_DISCARD);

	auto AddVertex = [&vertices, &offset](float x, float y, Color color) -> void {
		vertices[0].x = x;
		vertices[0].y = y;
		vertices[0].z = 0.0f;
		vertices[0].color = color;
		++vertices;
		++offset;
	};

	// === Star ===
	star_offset = offset;
	const float PI = 3.1415927f;
	float star_size_big = 300.0f;
	float star_size_small = 150.0f;
	for (unsigned int i = 0; i < star_ntriangles; ++i) {
		float angle1 = 2 * (i + 0) * PI / star_ntriangles;
		float angle2 = 2 * (i + 1) * PI / star_ntriangles;
		float size1 = ((i + 0) % 2 == 0 ? star_size_small : star_size_big);
		float size2 = ((i + 1) % 2 == 0 ? star_size_small : star_size_big);
		Color c1 = ((i + 0) % 2 == 0 ? color_quest : Colors::Sub(color_quest, color_modifier));
		Color c2 = ((i + 1) % 2 == 0 ? color_quest : Colors::Sub(color_quest, color_modifier));
		AddVertex(std::cos(angle1) * size1, std::sin(angle1) * size1, c1);
		AddVertex(std::cos(angle2) * size2, std::sin(angle2) * size2, c2);
		AddVertex(0.0f, 0.0f, Colors::Add(color_quest, color_modifier));
	}

	// === Arrow (quest) ===
	arrow_offset = offset;
	AddVertex(   0.0f, -125.0f, Colors::Add(color_quest, color_modifier));
	AddVertex( 250.0f, -250.0f, color_quest);
	AddVertex(   0.0f,  250.0f, color_quest);
	AddVertex(   0.0f,  250.0f, color_quest);
	AddVertex(-250.0f, -250.0f, color_quest);
	AddVertex(   0.0f, -125.0f, Colors::Add(color_quest, color_modifier));

	// === Arrow (north) ===
	north_offset = offset;
	AddVertex(   0.0f, -375.0f, Colors::Add(color_north, color_modifier));
	AddVertex( 250.0f, -500.0f, color_north);
	AddVertex(   0.0f,    0.0f, color_north);
	AddVertex(   0.0f,    0.0f, color_north);
	AddVertex(-250.0f, -500.0f, color_north);
	AddVertex(   0.0f, -375.0f, Colors::Add(color_north, color_modifier));

	buffer->Unlock();
}

void SymbolsRenderer::Render(IDirect3DDevice9* device) {
	if (!initialized) {
		initialized = true;
		Initialize(device);
	}

	GW::Agent* me = GW::Agents::GetPlayer();
	if (me == nullptr) return;

	device->SetFVF(D3DFVF_CUSTOMVERTEX);
	device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));

	const float PI = 3.1415927f;
	static float tau = 0.0f;
	tau += 0.05f;
	if (tau > 10 * PI) tau -= 10 * PI;
	D3DXMATRIX translate, scale, rotate, world;

	
	GW::QuestLog qlog = GW::GameContext::instance()->world->questlog;
	DWORD qid = GW::GameContext::instance()->world->activequestid;
	if (qlog.valid() && qid > 0) {
		GW::Vector2f qpos(0, 0);
		bool qfound = false;
		for (unsigned int i = 0; i < qlog.size(); ++i) {
			GW::Quest q = qlog[i];
			if (q.questid == qid) {
				qpos = GW::Vector2f(q.marker.x, q.marker.y);
				qfound = true;
				break;
			}
		}

		if (qfound) {
			const float compass_scale = Minimap::Instance().Scale();
			const float marker_scale = (1.0f / compass_scale);
			D3DXMatrixRotationZ(&rotate, -tau / 5);
			D3DXMatrixScaling(&scale, marker_scale + std::sin(tau) * 0.3f * marker_scale, marker_scale + std::sin(tau) * 0.3f * marker_scale, 1.0f);
			D3DXMatrixTranslation(&translate, qpos.x, qpos.y, 0);
			world = rotate * scale * translate;
			device->SetTransform(D3DTS_WORLD, &world);
			device->DrawPrimitive(type, star_offset, star_ntriangles);

			GW::Vector2f mypos = me->pos;
			GW::Vector2f v = qpos - mypos;
			const float max_quest_range = (GW::Constants::Range::Compass - 250.0f) / compass_scale;
			const float max_quest_range_sqr = max_quest_range * max_quest_range;
			if (v.SquaredNorm() > max_quest_range_sqr) {
				v = v.Normalized() * max_quest_range;
				
				float angle = std::atan2(v.y, v.x);
				D3DXMatrixRotationZ(&rotate, angle - (float)M_PI_2);
				D3DXMatrixScaling(&scale, marker_scale + std::sin(tau) * 0.3f * marker_scale, marker_scale + std::sin(tau) * 0.3f * marker_scale, 1.0f);
				D3DXMatrixTranslation(&translate, me->pos.x + v.x, me->pos.y + v.y, 0);
				world = rotate * scale * translate;
				device->SetTransform(D3DTS_WORLD, &world);
				device->DrawPrimitive(type, arrow_offset, arrow_ntriangles);
			}
		}
	}

	D3DXMatrixTranslation(&translate, me->pos.x, me->pos.y + 5000.0f, 0);
	world = translate;
	device->SetTransform(D3DTS_WORLD, &world);
	device->DrawPrimitive(type, north_offset, north_ntriangles);
}
