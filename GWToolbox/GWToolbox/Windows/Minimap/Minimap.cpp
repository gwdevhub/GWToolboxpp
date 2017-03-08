#include "Minimap.h"

#include <Windows.h>
#include <windowsx.h>
#include <thread>
#include <d3d9.h>
#include <d3dx9math.h>

#include <imgui_internal.h>
#include <ImGuiAddons.h>
#include <GWCA\GWCA.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA\Managers\CameraMgr.h>
#include "logger.h"
#include "OtherModules\ToolboxSettings.h"

void Minimap::Initialize() {
	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P041>(
		[&](GW::Packet::StoC::P041* pak) -> bool {
		if (visible) {
			pingslines_renderer.P041Callback(pak);
		}
		return false;
	});
	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P133>(
		[&](GW::Packet::StoC::P133* pak) -> bool {
		if (visible) {
			pingslines_renderer.P133Callback(pak);
		}
		return false;
	});
	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P148>(
		[&](GW::Packet::StoC::P148* pak) -> bool {
		if (visible) {
			pingslines_renderer.P148Callback(pak);
		}
		return false;
	});
	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P216>(
		[&](GW::Packet::StoC::P216* pak) -> bool {
		if (visible) {
			pingslines_renderer.P216Callback(pak);
		}
		return false;
	});
	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P391_InstanceLoadFile>(
		[this](GW::Packet::StoC::P391_InstanceLoadFile* packet) -> bool {
		pmap_renderer.Invalidate();
		loading = false;
		return false;
	});

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P406>(
		[&](GW::Packet::StoC::P406* pak) -> bool {
		loading = true;
		return false;
	});

	last_moved = TIMER_INIT();

	pmap_renderer.Invalidate();
}

void Minimap::DrawSettings() {
	if (ImGui::CollapsingHeader(Name(), ImGuiTreeNodeFlags_AllowOverlapMode)) {
		ImGui::PushID(Name());
		ShowVisibleRadio();
		ImVec2 pos(0, 0);
		ImVec2 size(0, 0);
		if (ImGuiWindow* window = ImGui::FindWindowByName(Name())) {
			pos = window->Pos;
			size = window->Size;
		}
		if (ImGui::DragFloat2("Position", (float*)&pos, 1.0f, 0.0f, 0.0f, "%.0f")) {
			ImGui::SetWindowPos(Name(), pos);
		}
		ImGui::ShowHelp("You need to show the window for this control to work");
		if (ImGui::DragFloat2("Size", (float*)&size, 1.0f, 0.0f, 0.0f, "%.0f")) {
			ImGui::SetWindowSize(Name(), size);
		}
		ImGui::ShowHelp("You need to show the window for this control to work");
		ImGui::Checkbox("Lock Position", &lock_move);
		ImGui::SameLine();
		ImGui::Checkbox("Lock Size", &lock_size);
		DrawSettingInternal();
		ImGui::PopID();
	} else {
		ShowVisibleRadio();
	}
}

void Minimap::DrawSettingInternal() {
	ImGui::Text("General");
	ImGui::DragFloat("Scale", &scale, 0.01f, 0.1f);
	ImGui::Text("You can set the color alpha to 0 to disable any minimap feature");
	if (ImGui::TreeNode("Agents")) {
		agent_renderer.DrawSettings();
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Ranges")) {
		range_renderer.DrawSettings();
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Pings and drawings")) {
		pingslines_renderer.DrawSettings();
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Symbols")) {
		symbols_renderer.DrawSettings();
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Map")) {
		pmap_renderer.DrawSettings();
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Custom Markers")) {
		custom_renderer.DrawSettings();
		ImGui::TreePop();
	}
}

void Minimap::LoadSettings(CSimpleIni* ini) {
	ToolboxWidget::LoadSettings(ini);
	scale = (float)ini->GetDoubleValue(Name(), "scale", 1.0);
	range_renderer.LoadSettings(ini, Name());
	pmap_renderer.LoadSettings(ini, Name());
	agent_renderer.LoadSettings(ini, Name());
	pingslines_renderer.LoadSettings(ini, Name());
	symbols_renderer.LoadSettings(ini, Name());
	custom_renderer.LoadSettings(ini, Name());
}

void Minimap::SaveSettings(CSimpleIni* ini) {
	ToolboxWidget::SaveSettings(ini);
	ini->SetDoubleValue(Name(), "scale", scale);
	range_renderer.SaveSettings(ini, Name());
	pmap_renderer.SaveSettings(ini, Name());
	agent_renderer.SaveSettings(ini, Name());
	pingslines_renderer.SaveSettings(ini, Name());
	symbols_renderer.SaveSettings(ini, Name());
	custom_renderer.SaveSettings(ini, Name());
}

void Minimap::Draw(IDirect3DDevice9* device) {
	if (!IsActive()) return;

	GW::Agent* me = GW::Agents().GetPlayer();
	if (me == nullptr) return;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(0, 0, 0, 0));
	ImGui::SetNextWindowSize(ImVec2(500.0f, 500.0f), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), nullptr, GetWinFlags())) {
		location.x = ImGui::GetWindowPos().x;
		location.y = ImGui::GetWindowPos().y;
		size.x = ImGui::GetWindowSize().x;
		size.y = ImGui::GetWindowSize().y;
	}
	ImGui::End();
	ImGui::PopStyleColor();

	// if not center and want to move, move center towards player
	if ((translation.x != 0 || translation.y != 0)
		&& (me->MoveX != 0 || me->MoveY != 0)
		&& TIMER_DIFF(last_moved) > ms_before_back) {
		GW::Vector2f v(translation.x, translation.y);
		float speed = std::min((TIMER_DIFF(last_moved) - ms_before_back) * acceleration, 500.0f);
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
	clipping.right = location.x + size.x + 1;
	clipping.top = location.y > 0 ? location.y : 0;
	clipping.bottom = location.y + size.y + 1;
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

	custom_renderer.Render(device);

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
	v.x = (2.0f * v.x / size.x - 1.0f);
	v.y = (2.0f * v.y / size.x + 1.0f);

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
	v.x = (2.0f * v.x / size.x);
	v.y = (2.0f * v.y / size.x);

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

	if (!lock_move) return true;

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
		last_moved = TIMER_INIT();
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

	return false;
}

bool Minimap::IsInside(int x, int y) const {
	// if outside square, return false
	if (x < location.x) return false;
	if (x > location.x + size.x) return false;
	if (y < location.y) return false;
	if (y > location.y + size.y) return false;

	// if centered, use radar range
	if (translation.x == 0 && translation.y == 0) {
		GW::Vector2f gamepos = InterfaceToWorldPoint(Vec2i(x, y));
		GW::Agent* me = GW::Agents().GetPlayer();
		float sqrdst = GW::Agents().GetSqrDistance(me->pos, gamepos);
		return me && sqrdst < GW::Constants::SqrRange::Compass;
	}
	return true;
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
	float xscale = (float)size.x / viewport.Width;
	float yscale = (float)size.x / viewport.Height;
	float xtrans = (float)(location.x * 2 + size.x) / viewport.Width - 1.0f;
	float ytrans = -(float)(location.y * 2 + size.x) / viewport.Height + 1.0f;
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
