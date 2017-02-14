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

Minimap::Minimap() 
	: range_renderer(RangeRenderer()),
	pmap_renderer(PmapRenderer()),
	agent_renderer(AgentRenderer()),
	symbols_renderer(SymbolsRenderer()),
	mousedown_(false),
	freeze_(false),
	loading_(false),
	visible_(false),
	mapfile(0) {

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P391_InstanceLoadFile>(
		[this](GW::Packet::StoC::P391_InstanceLoadFile* packet) -> bool {
		mapfile = packet->map_fileID;
		pmap_renderer.Invalidate();
		loading_ = false;
		return false;
	});

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P406>(
		[&](GW::Packet::StoC::P406* pak) -> bool {
		loading_ = true;
		return false;
	});

	int x = Config::IniRead(Minimap::IniSection(), Minimap::IniKeyX(), 50l);
	int y = Config::IniRead(Minimap::IniSection(), Minimap::IniKeyY(), 50l);
	int size = Config::IniRead(Minimap::IniSection(), Minimap::IniKeySize(), 600l);
	double scale = Config::IniRead(Minimap::IniSection(), Minimap::IniKeyScale(), 1.0);
	SetLocation(x, y);
	SetSize(size, size);

	SetTranslation(0.0f, 0.0f);
	SetScale((float)scale);
	last_moved_ = TBTimer::init();

	SetVisible(Config::IniRead(Minimap::IniSection(), Minimap::InikeyShow(), false));

	pmap_renderer.Invalidate();
}

void Minimap::Render(IDirect3DDevice9* device) {
	if (!IsActive()) return;

	GW::Agent* me = GW::Agents().GetPlayer();
	if (me == nullptr) return;

	const long ms_before_back = 1000;
	const float acceleration = 0.5f;
	const float max_speed = 15.0f; // game units per frame
	if ((translation_x_ != 0 || translation_y_ != 0) 
		&& (me->MoveX != 0 || me->MoveY != 0)
		&& TBTimer::diff(last_moved_) > ms_before_back) {
		GW::Vector2f v(translation_x_, translation_y_);
		float speed = std::min((TBTimer::diff(last_moved_) - ms_before_back) * acceleration, 500.0f);
		float n = v.Norm();
		GW::Vector2f d = v.Normalized() * speed;
		if (std::abs(d.x) > std::abs(v.x)) SetTranslation(0, 0);
		else Translate(-d.x, -d.y);
	}

	D3DXMATRIX old_view, old_world, old_projection;
	DWORD old_fvf, old_mutlisample, old_fillmode, old_lighting, old_scissortest;
	device->GetTransform(D3DTS_WORLD, &old_world);
	device->GetTransform(D3DTS_VIEW, &old_view);
	device->GetTransform(D3DTS_PROJECTION, &old_projection);
	device->GetFVF(&old_fvf);
	device->SetFVF(D3DFVF_CUSTOMVERTEX);
	device->GetRenderState(D3DRS_MULTISAMPLEANTIALIAS, &old_mutlisample);
	device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);
	device->GetRenderState(D3DRS_FILLMODE, &old_fillmode);
	device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	device->GetRenderState(D3DRS_LIGHTING, &old_lighting);
	device->SetRenderState(D3DRS_LIGHTING, FALSE);	
	device->GetRenderState(D3DRS_SCISSORTESTENABLE, &old_scissortest);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);

	RECT old_clipping, clipping;
	device->GetScissorRect(&old_clipping);
	clipping.left = location_x_ < 0 ? 0 : location_x_;
	clipping.right = location_x_ + width_ + 1;
	clipping.top = location_y_ < 0 ? 0 : location_y_;
	clipping.bottom = location_y_ + height_ + 1;
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

	D3DXMATRIX scale, trans;
	D3DXMatrixScaling(&scale, scale_, scale_, 1.0f);
	D3DXMatrixTranslation(&trans, translation_x_, translation_y_, 0);

	view = translate_char * rotate_char * scale * trans;
	device->SetTransform(D3DTS_VIEW, &view);

	pmap_renderer.Render(device);	

	// move the rings to the char position
	D3DXMatrixTranslation(&translate_char, me->X, me->Y, 0);
	device->SetTransform(D3DTS_WORLD, &translate_char); 
	range_renderer.Render(device);
	device->SetTransform(D3DTS_WORLD, &identity);

	if (translation_x_ != 0 || translation_y_ != 0) {
		D3DXMATRIX view2 = scale;
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

	device->SetFVF(old_fvf);
	device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, old_mutlisample);
	device->SetRenderState(D3DRS_FILLMODE, old_fillmode);
	device->SetRenderState(D3DRS_LIGHTING, old_lighting);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, old_scissortest);
	device->SetScissorRect(&old_clipping);
	device->SetTransform(D3DTS_WORLD, &old_world);
	device->SetTransform(D3DTS_VIEW, &old_view);
	device->SetTransform(D3DTS_PROJECTION, &old_projection);
}

GW::Vector2f Minimap::InterfaceToWorldPoint(int x, int y) const {
	GW::Agent* me = GW::Agents().GetPlayer();
	if (me == nullptr) return GW::Vector2f(0, 0);
	
	// Invert viewport projection
	x = x - GetX();
	y = GetY() - y;

	// go from [0, width][0, height] to [-1, 1][-1, 1]
	float x2 = (2.0f * x / GetWidth() - 1.0f);
	float y2 = (2.0f * y / GetHeight() + 1.0f);

	// scale up to [-w, w]
	float w = 5000.0f;
	x2 *= w;
	y2 *= w;

	// translate by camera
	x2 -= translation_x_;
	y2 -= translation_y_;

	// scale by camera
	x2 /= scale_;
	y2 /= scale_;

	// rotate by current camera rotation
	float angle = GW::Cameramgr().GetYaw() - (float)M_PI_2;
	float x3 = x2 * std::cos(angle) - y2 * std::sin(angle);
	float y3 = x2 * std::sin(angle) + y2 * std::cos(angle);

	// translate by character position
	x3 += me->X;
	y3 += me->Y;

	return GW::Vector2f(x3, y3);
}

GW::Vector2f Minimap::InterfaceToWorldVector(int x, int y) const {
	// Invert y direction
	y = -y;

	// go from [0, width][0, height] to [-1, 1][-1, 1]
	float x2 = (2.0f * x / GetWidth());
	float y2 = (2.0f * y / GetHeight());

	// scale up to [-w, w]
	float w = 5000.0f;
	x2 *= w;
	y2 *= w;

	return GW::Vector2f(x2, y2);
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

bool Minimap::OnMouseDown(MSG msg) {
	if (!IsActive()) return false;

	int x = GET_X_LPARAM(msg.lParam);
	int y = GET_Y_LPARAM(msg.lParam);
	if (!IsInside(x, y)) return false;

	mousedown_ = true;

	if (msg.wParam & MK_CONTROL) {
		SelectTarget(InterfaceToWorldPoint(x, y));
		return true;
	}

	drag_start_x_ = x;
	drag_start_y_ = y;

	if (msg.wParam & MK_SHIFT) return true;

	if (!freeze_) return true;

	GW::Vector2f v = InterfaceToWorldPoint(x, y);
	pingslines_renderer.OnMouseDown(v.x, v.y);

	return true;
}

bool Minimap::OnMouseDblClick(MSG msg) {
	if (!IsActive()) return false;

	int x = GET_X_LPARAM(msg.lParam);
	int y = GET_Y_LPARAM(msg.lParam);
	if (!IsInside(x, y)) return false;

	if (msg.wParam & MK_CONTROL) {
		SelectTarget(InterfaceToWorldPoint(x, y));
		return true;
	}

	return true;
}

bool Minimap::OnMouseUp(MSG msg) {
	if (!IsActive()) return false;

	if (!mousedown_) return false;

	mousedown_ = false;
	
	if (!freeze_) {
		Config::IniWrite(Minimap::IniSection(), Minimap::IniKeyX(), GetX());
		Config::IniWrite(Minimap::IniSection(), Minimap::IniKeyY(), GetY());
	}

	return pingslines_renderer.OnMouseUp();
}

bool Minimap::OnMouseMove(MSG msg) {
	if (!IsActive()) return false;

	if (!mousedown_) return false;
	
	int x = GET_X_LPARAM(msg.lParam);
	int y = GET_Y_LPARAM(msg.lParam);
	//if (!IsInside(x, y)) return false;

	if (msg.wParam & MK_CONTROL) {
		SelectTarget(InterfaceToWorldPoint(x, y));
		return true;
	}

	if (msg.wParam & MK_SHIFT) {
		int diff_x = x - drag_start_x_;
		int diff_y = y - drag_start_y_;
		GW::Vector2f trans = InterfaceToWorldVector(diff_x, diff_y);
		Translate(trans.x, trans.y);
		drag_start_x_ = x;
		drag_start_y_ = y;
		last_moved_ = TBTimer::init();
		return true;
	}

	if (!freeze_) {
		int diff_x = x - drag_start_x_;
		int diff_y = y - drag_start_y_;
		SetX(GetX() + diff_x);
		SetY(GetY() + diff_y);
		drag_start_x_ = x;
		drag_start_y_ = y;
		return true;
	}

	GW::Vector2f v = InterfaceToWorldPoint(x, y);
	return pingslines_renderer.OnMouseMove(v.x, v.y);
}

bool Minimap::OnMouseWheel(MSG msg) {
	if (!IsActive()) return false;

	int x = GET_X_LPARAM(msg.lParam);
	int y = GET_Y_LPARAM(msg.lParam);
	if (!IsInside(x, y)) return false;
	
	int zDelta = GET_WHEEL_DELTA_WPARAM(msg.wParam);

	if (msg.wParam & MK_SHIFT) {
		float delta = zDelta > 0 ? 1.024f : 0.9765625f;
		Scale(delta);
		Config::IniWrite(Minimap::IniSection(), Minimap::IniKeyScale(), GetScale());
		return true;
	}

	if (!freeze_) {
		int delta = zDelta > 0 ? 2 : -2;
		SetWidth(GetWidth() + delta * 2);
		SetHeight(GetHeight() + delta * 2);
		SetX(GetX() - delta);
		SetY(GetY() - delta);

		Config::IniWrite(Minimap::IniSection(), Minimap::IniKeySize(), GetWidth());

		return true;
	}
	return false;
}

bool Minimap::IsInside(int x, int y) const {
	// if centered, use radar range, otherwise use square
	if (translation_x_ == 0 && translation_y_ == 0) {
		GW::Vector2f gamepos = InterfaceToWorldPoint(x, y);
		GW::Agent* me = GW::Agents().GetPlayer();
		return me && GW::Agents().GetSqrDistance(me->pos, gamepos) < GW::Constants::SqrRange::Compass;
	} else {
		return (x >= GetX() && x < GetX() + GetWidth()
			&& y >= GetY() && y < GetY() + GetHeight());
	}
}
bool Minimap::IsActive() const {
	return visible_
		&& !loading_
		&& GW::Map().IsMapLoaded()
		&& GW::Map().GetInstanceType() != GW::Constants::InstanceType::Loading
		&& GW::Agents().GetPlayerId() != 0;
}
