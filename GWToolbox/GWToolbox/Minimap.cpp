#include "Minimap.h"

#include <Windows.h>
#include <windowsx.h>
#include <thread>
#include <d3d9.h>
#include <d3dx9math.h>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA\Managers\CameraMgr.h>
#include "Config.h"
#include "logger.h"
#include "GWToolbox.h"

Minimap::Minimap() 
	: range_renderer(RangeRenderer()),
	pmap_renderer(PmapRenderer()),
	agent_renderer(AgentRenderer()),
	symbols_renderer(SymbolsRenderer()),
	mousedown(false),
	loading(false),
	mapfile(0),
	translation(0, 0),
	location(0, 0) {

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P391_InstanceLoadFile>(
		[this](GW::Packet::StoC::P391_InstanceLoadFile* packet) -> bool {
		mapfile = packet->map_fileID;
		pmap_renderer.Invalidate();
		loading = false;
		return false;
	});

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P406>(
		[&](GW::Packet::StoC::P406* pak) -> bool {
		loading = true;
		return false;
	});

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P133>(
		[&](GW::Packet::StoC::P133* pak) -> bool {
		if (visible) {
			pingslines_renderer.P133Callback(pak);
		}
		return false;
	});

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P041>(
		[&](GW::Packet::StoC::P041* pak) -> bool {
		if (visible) {
			pingslines_renderer.P041Callback(pak);
		}
		return false;
	});

	last_moved = TBTimer::init();

	pmap_renderer.Invalidate();
}

void Minimap::DrawSettings() {
	if (ImGui::CollapsingHeader(Name())) {
		ImGui::Text("General");
		ImGui::DragInt("X", &location.x);
		ImGui::DragInt("Y", &location.y);
		ImGui::DragInt("Size", &size);
		ImGui::DragFloat("Scale", &scale);
		ImGui::Text("Agents");
		agent_renderer.DrawSettings();
		ImGui::Text("Ranges");
		range_renderer.DrawSettings();
		ImGui::Text("Pings and drawings");
		pingslines_renderer.DrawSettings();
		ImGui::Text("Symbols");
		symbols_renderer.DrawSettings();
		ImGui::Text("Map");
		pmap_renderer.DrawSettings();
	}
}

void Minimap::LoadSettings(CSimpleIni* ini) {
	location.x = ini->GetLongValue(Name(), "x", 50);
	location.y = ini->GetLongValue(Name(), "y", 50);
	size = ini->GetLongValue(Name(), "size", 600);
	scale = (float)ini->GetDoubleValue(Name(), "scale", 1.0);
	LoadSettingVisible(ini);
	range_renderer.LoadSettings(ini, Name());
	pmap_renderer.LoadSettings(ini, Name());
	agent_renderer.LoadSettings(ini, Name());
	pingslines_renderer.LoadSettings(ini, Name());
	symbols_renderer.LoadSettings(ini, Name());
}

void Minimap::SaveSettings(CSimpleIni* ini) const {
	ini->SetLongValue(Name(), "x", location.x);
	ini->SetLongValue(Name(), "y", location.y);
	ini->SetLongValue(Name(), "size", size);
	ini->SetDoubleValue(Name(), "scale", scale);
	SaveSettingVisible(ini);
	range_renderer.SaveSettings(ini, Name());
	pmap_renderer.SaveSettings(ini, Name());
	agent_renderer.SaveSettings(ini, Name());
	pingslines_renderer.SaveSettings(ini, Name());
	symbols_renderer.SaveSettings(ini, Name());
}

void Minimap::Draw(IDirect3DDevice9* device) {
	if (!IsActive()) return;

	GW::Agent* me = GW::Agents().GetPlayer();
	if (me == nullptr) return;

	// if not center and want to move, move center towards player
	if ((translation.x != 0 || translation.y != 0) 
		&& (me->MoveX != 0 || me->MoveY != 0)
		&& TBTimer::diff(last_moved) > ms_before_back) {
		GW::Vector2f v(translation.x, translation.y);
		float speed = std::min((TBTimer::diff(last_moved) - ms_before_back) * acceleration, 500.0f);
		float n = v.Norm();
		GW::Vector2f d = v.Normalized() * speed;
		if (std::abs(d.x) > std::abs(v.x)) {
			translation = GW::Vector2f(0, 0);
		} else {
			translation -= d;
		}
	}

	// Backup the DX9 state
	IDirect3DStateBlock9* d3d9_state_block = NULL;
	if (device->CreateStateBlock(D3DSBT_ALL, &d3d9_state_block) < 0)
		return;

	// Setup render state: fixed-pipeline, alpha-blending, no face culling, no depth testing
	device->SetFVF(D3DFVF_CUSTOMVERTEX);
	device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, true);
	device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	device->SetPixelShader(NULL);
	device->SetVertexShader(NULL);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_LIGHTING, false);
	device->SetRenderState(D3DRS_ZENABLE, false);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
	device->SetRenderState(D3DRS_ALPHATESTENABLE, false);
	device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, true);
	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

	RECT old_clipping, clipping;
	device->GetScissorRect(&old_clipping);
	clipping.left = location.x > 0 ? location.x : 0;
	clipping.right = location.x + size + 1;
	clipping.top = location.y > 0 ? location.y : 0;
	clipping.bottom = location.y + size + 1;
	device->SetScissorRect(&clipping);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
	RenderSetupProjection(device);

	D3DXMATRIX view;
	D3DXMATRIX identity;
	D3DXMatrixIdentity(&identity);
	device->SetTransform(D3DTS_WORLD, &identity);
	device->SetTransform(D3DTS_VIEW, &identity);

	D3DXMATRIX translate_char;
	D3DXMatrixTranslation(&translate_char, -me->X, -me->Y, 0);
	
	D3DXMATRIX rotate_char;
	D3DXMatrixRotationZ(&rotate_char, -GW::Cameramgr().GetYaw() + (float)M_PI_2);

	D3DXMATRIX scaleM, translationM;
	D3DXMatrixScaling(&scaleM, scale, scale, 1.0f);
	D3DXMatrixTranslation(&translationM, translation.x, translation.y, 0);

	view = translate_char * rotate_char * scaleM * translationM;
	device->SetTransform(D3DTS_VIEW, &view);

	pmap_renderer.Render(device);	

	// move the rings to the char position
	D3DXMatrixTranslation(&translate_char, me->X, me->Y, 0);
	device->SetTransform(D3DTS_WORLD, &translate_char); 
	range_renderer.Render(device);
	device->SetTransform(D3DTS_WORLD, &identity);

	if (translation.x != 0 || translation.y != 0) {
		D3DXMATRIX view2 = scaleM;
		device->SetTransform(D3DTS_VIEW, &view2);
		range_renderer.SetDrawCenter(true);
		range_renderer.Render(device);
		range_renderer.SetDrawCenter(false);
		device->SetTransform(D3DTS_VIEW, &view);
	}

	symbols_renderer.Render(device);
	
	device->SetTransform(D3DTS_WORLD, &identity);
	agent_renderer.Render(device);

	pingslines_renderer.Render(device);

	// Restore the DX9 state
	d3d9_state_block->Apply();
	d3d9_state_block->Release();
}

GW::Vector2f Minimap::InterfaceToWorldPoint(Vec2i pos) const {
	GW::Agent* me = GW::Agents().GetPlayer();
	if (me == nullptr) return GW::Vector2f(0, 0);
	
	GW::Vector2f v((float)pos.x, (float)pos.y);

	// Invert viewport projection
	v.x = v.x - location.x;
	v.y = location.y - v.y;

	// go from [0, width][0, height] to [-1, 1][-1, 1]
	v.x = (2.0f * v.x / size - 1.0f);
	v.y = (2.0f * v.y / size + 1.0f);

	// scale up to [-w, w]
	float w = 5000.0f;
	v *= w;

	// translate by camera
	v -= translation;

	// scale by camera
	v /= scale;

	// rotate by current camera rotation
	float angle = GW::Cameramgr().GetYaw() - (float)M_PI_2;
	float x1 = v.x * std::cos(angle) - v.y * std::sin(angle);
	float y1 = v.x * std::sin(angle) + v.y * std::cos(angle);
	v = GW::Vector2f(x1, y1);

	// translate by character position
	v += GW::Vector2f(me->X, me->Y);

	return v;
}

GW::Vector2f Minimap::InterfaceToWorldVector(Vec2i pos) const {
	GW::Vector2f v((float)pos.x, (float)pos.y);

	// Invert y direction
	v.y = -v.y;

	// go from [0, width][0, height] to [-1, 1][-1, 1]
	v.x = (2.0f * v.x / size);
	v.y = (2.0f * v.y / size);

	// scale up to [-w, w]
	float w = 5000.0f;
	v *= w;

	return v;
}

void Minimap::SelectTarget(GW::Vector2f pos) {
	GW::AgentArray agents = GW::Agents().GetAgentArray();
	if (!agents.valid()) return;

	float distance = 600.0f * 600.0f;
	int closest = -1;

	for (size_t i = 0; i < agents.size(); ++i) {
		GW::Agent* agent = agents[i];
		if (agent == nullptr) continue;
		if (agent->GetIsLivingType() && agent->GetIsDead()) continue;
		if (agent->GetIsItemType()) continue;
		if (agent->GetIsSignpostType() && agent->ExtraType != 8141) continue; // allow locked chests
		float newDistance = GW::Agents().GetSqrDistance(pos, agents[i]->pos);
		if (distance > newDistance) {
			distance = newDistance;
			closest = i;
		}
	}

	if (closest > 0) {
		GW::Agents().ChangeTarget(agents[closest]);
	}
}

bool Minimap::WndProc(UINT Message, WPARAM wParam, LPARAM lParam) {
	switch (Message) {
	case WM_MOUSEMOVE: return OnMouseMove(Message, wParam, lParam);
	case WM_LBUTTONDOWN: return OnMouseDown(Message, wParam, lParam);
	case WM_MOUSEWHEEL: return OnMouseWheel(Message, wParam, lParam);
	case WM_LBUTTONDBLCLK: return OnMouseDblClick(Message, wParam, lParam);
	case WM_LBUTTONUP: return OnMouseUp(Message, wParam, lParam);
	default:
		return false;
	}
}
bool Minimap::OnMouseDown(UINT Message, WPARAM wParam, LPARAM lParam) {
	if (!IsActive()) return false;

	int x = GET_X_LPARAM(lParam);
	int y = GET_Y_LPARAM(lParam);
	if (!IsInside(x, y)) return false;

	mousedown = true;

	if (wParam & MK_CONTROL) {
		SelectTarget(InterfaceToWorldPoint(Vec2i(x, y)));
		return true;
	}

	drag_start.x = x;
	drag_start.y = y;

	if (wParam & MK_SHIFT) return true;

	if (!GWToolbox::instance().other_settings->freeze_widgets) return true;

	GW::Vector2f v = InterfaceToWorldPoint(Vec2i(x, y));
	pingslines_renderer.OnMouseDown(v.x, v.y);

	return true;
}

bool Minimap::OnMouseDblClick(UINT Message, WPARAM wParam, LPARAM lParam) {
	if (!IsActive()) return false;

	int x = GET_X_LPARAM(lParam);
	int y = GET_Y_LPARAM(lParam);
	if (!IsInside(x, y)) return false;

	if (wParam & MK_CONTROL) {
		SelectTarget(InterfaceToWorldPoint(Vec2i(x, y)));
		return true;
	}

	return true;
}

bool Minimap::OnMouseUp(UINT Message, WPARAM wParam, LPARAM lParam) {
	if (!IsActive()) return false;

	if (!mousedown) return false;

	mousedown = false;

	return pingslines_renderer.OnMouseUp();
}

bool Minimap::OnMouseMove(UINT Message, WPARAM wParam, LPARAM lParam) {
	if (!IsActive()) return false;

	if (!mousedown) return false;
	
	int x = GET_X_LPARAM(lParam);
	int y = GET_Y_LPARAM(lParam);
	//if (!IsInside(x, y)) return false;

	if (wParam & MK_CONTROL) {
		SelectTarget(InterfaceToWorldPoint(Vec2i(x, y)));
		return true;
	}

	if (wParam & MK_SHIFT) {
		Vec2i diff = Vec2i(x - drag_start.x, y - drag_start.y);
		translation += InterfaceToWorldVector(diff);
		drag_start = Vec2i(x, y);
		last_moved = TBTimer::init();
		return true;
	}

	if (!GWToolbox::instance().other_settings->freeze_widgets) {
		int diff_x = x - drag_start.x;
		int diff_y = y - drag_start.y;
		location.x += diff_x;
		location.y += diff_y;
		drag_start = Vec2i(x, y);
		return true;
	}

	GW::Vector2f v = InterfaceToWorldPoint(Vec2i(x, y));
	return pingslines_renderer.OnMouseMove(v.x, v.y);
}

bool Minimap::OnMouseWheel(UINT Message, WPARAM wParam, LPARAM lParam) {
	if (!IsActive()) return false;

	int x = GET_X_LPARAM(lParam);
	int y = GET_Y_LPARAM(lParam);
	if (!IsInside(x, y)) return false;
	
	int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

	if (wParam & MK_SHIFT) {
		float delta = zDelta > 0 ? 1.024f : 0.9765625f;
		scale *= delta;
		return true;
	}

	if (!GWToolbox::instance().other_settings->freeze_widgets) {
		int delta = zDelta > 0 ? 2 : -2;
		size += delta * 2;
		size += delta * 2;
		location.x -= delta;
		location.y -= delta;

		return true;
	}
	return false;
}

bool Minimap::IsInside(int x, int y) const {
	// if centered, use radar range, otherwise use square
	if (translation.x == 0 && translation.y == 0) {
		GW::Vector2f gamepos = InterfaceToWorldPoint(Vec2i(x, y));
		GW::Agent* me = GW::Agents().GetPlayer();
		return me && GW::Agents().GetSqrDistance(me->pos, gamepos) < GW::Constants::SqrRange::Compass;
	} else {
		return (x >= location.x && x < location.x + size
			&& y >= location.y && y < location.y + size);
	}
}
bool Minimap::IsActive() const {
	return visible
		&& !loading
		&& GW::Map().IsMapLoaded()
		&& GW::Map().GetInstanceType() != GW::Constants::InstanceType::Loading
		&& GW::Agents().GetPlayerId() != 0;
}

void Minimap::RenderSetupProjection(IDirect3DDevice9* device) {
	D3DVIEWPORT9 viewport;
	device->GetViewport(&viewport);

	float w = 5000.0f * 2;
	// IMPORTANT: we are setting z-near to 0.0f and z-far to 1.0f
	D3DXMATRIX ortho_matrix(
		2 / w, 0, 0, 0,
		0, 2 / w, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);

	//// note: manually craft the projection to viewport instead of using
	//// SetViewport to allow target regions outside the viewport 
	//// e.g. negative x/y for slightly offscreen map
	float xscale = (float)size / viewport.Width;
	float yscale = (float)size / viewport.Height;
	float xtrans = (float)(location.x * 2 + size) / viewport.Width - 1.0f;
	float ytrans = -(float)(location.y * 2 + size) / viewport.Height + 1.0f;
	////IMPORTANT: we are basically setting z-near to 0 and z-far to 1
	D3DXMATRIX viewport_matrix(
		xscale, 0, 0, 0,
		0, yscale, 0, 0,
		0, 0, 1, 0,
		xtrans, ytrans, 0, 1
	);

	D3DXMATRIX proj = ortho_matrix * viewport_matrix;

	device->SetTransform(D3DTS_PROJECTION, &proj);
}
