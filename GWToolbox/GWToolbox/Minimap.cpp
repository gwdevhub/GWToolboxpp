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
	mousedown_(false),
	freeze_(false),
	loading_(false),
	visible_(false) {

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P391_InstanceLoadFile>(
		[this](GW::Packet::StoC::P391_InstanceLoadFile* packet) {
		printf("loading map %d\n", packet->map_fileID);
		pmap_renderer.Invalidate();
		loading_ = false;
		return false;
	});

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P406>(
		[&](GW::Packet::StoC::P406* pak) {
		loading_ = true;
		return false;
	});

	int x = Config::IniReadLong(Minimap::IniSection(), Minimap::IniKeyX(), 50);
	int y = Config::IniReadLong(Minimap::IniSection(), Minimap::IniKeyY(), 50);
	int width = Config::IniReadLong(Minimap::IniSection(), Minimap::IniKeyWidth(), 600);
	SetX(x);
	SetY(y);
	SetWidth(width);
	SetHeight(width);

	Scale(0.0002f);

	SetVisible(Config::IniReadBool(Minimap::IniSection(), Minimap::InikeyShow(), false));

	pmap_renderer.Invalidate();
}

void Minimap::Render(IDirect3DDevice9* device) {
	if (!visible_) return;

	if (loading_
		|| !GW::Map().IsMapLoaded()
		|| GW::Map().GetInstanceType() == GW::Constants::InstanceType::Loading
		|| GW::Agents().GetPlayerId() == 0) {
		return;
	}

	GW::Agent* me = GW::Agents().GetPlayer();
	if (me == nullptr) return;
	SetTranslation(-me->X, -me->Y);

	float camera_yaw = GW::Cameramgr().GetYaw();
	SetRotation(-camera_yaw + (float)M_PI_2);

	device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);
	device->SetFVF(D3DFVF_CUSTOMVERTEX);
	device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	device->SetRenderState(D3DRS_LIGHTING, FALSE);

	D3DXMATRIX identity;
	D3DXMatrixIdentity(&identity);
	device->SetTransform(D3DTS_WORLD, &identity);
	device->SetTransform(D3DTS_VIEW, &identity);
	device->SetTransform(D3DTS_PROJECTION, &identity);

	RenderSetupClipping(device);
	RenderSetupProjection(device);
	//HRESULT zenable = device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
	//if (zenable != D3D_OK) {
	//	printf("NOT OK\n");
	//}
	//device->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0x00000000, 0.5f, 0);
	//if (zenable != D3D_OK) {
	//	printf("NOT OK2\n");
	//}
	//D3DCAPS9 caps;
	//device->GetDeviceCaps(&caps);

	//LPDIRECT3DSURFACE9 pZBuffer;
	//device->GetDepthStencilSurface(&pZBuffer);

	RenderSetupWorldTransforms(device, false, false, true);
	pmap_renderer.Render(device);	

	RenderSetupWorldTransforms(device, false, false, true);
	range_renderer.Render(device);

	RenderSetupWorldTransforms(device, true, true, true);

	agent_renderer.Render(device);

	pingslines_renderer.Render(device);
}

GW::Vector2f Minimap::InterfaceToWorld(int x, int y) const {
	GW::Agent* me = GW::Agents().GetPlayer();
	if (me == nullptr) return GW::Vector2f(0, 0);

	x -= GetX();
	y -= GetY();

	// go from [0, width][0, height] to [-1, 1][-1, 1] and then divide by scale
	float xs = (2.0f * x / GetWidth() - 1.0f) / GetScale();
	float ys = (2.0f * y / GetHeight() - 1.0f) / GetScale();

	// rotate by current camera rotation
	float angle = -GW::Cameramgr().GetYaw() + (float)M_PI_2;
	float xr = xs * std::cos(angle) - ys * std::sin(angle);
	float yr = ys * std::cos(angle) + xs * std::sin(angle);

	// translate by character position
	float xt = xr - GetTranslationX();
	float yt = -yr - GetTranslationY();

	return GW::Vector2f(xt, yt);
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
	if (!visible_) return false;

	int x = GET_X_LPARAM(msg.lParam);
	int y = GET_Y_LPARAM(msg.lParam);
	if (!IsInside(x, y)) return false;

	mousedown_ = true;

	if (msg.wParam & MK_CONTROL) {
		SelectTarget(InterfaceToWorld(x, y));
		return true;
	}

	if (!freeze_) {
		drag_start_x_ = x;
		drag_start_y_ = y;
		return true;
	}

	GW::Vector2f v = InterfaceToWorld(x, y);
	return pingslines_renderer.OnMouseDown(v.x, v.y);
}

bool Minimap::OnMouseDblClick(MSG msg) {
	if (!visible_) return false;

	int x = GET_X_LPARAM(msg.lParam);
	int y = GET_Y_LPARAM(msg.lParam);
	if (!IsInside(x, y)) return false;

	if (msg.wParam & MK_CONTROL) {
		SelectTarget(InterfaceToWorld(x, y));
		return true;
	}

	return true;
}

bool Minimap::OnMouseUp(MSG msg) {
	if (!visible_) return false;
	if (!mousedown_) return false;

	mousedown_ = false;
	
	if (!freeze_) {
		Config::IniWriteLong(Minimap::IniSection(), Minimap::IniKeyX(), GetX());
		Config::IniWriteLong(Minimap::IniSection(), Minimap::IniKeyY(), GetY());
	}

	return pingslines_renderer.OnMouseUp();
}

bool Minimap::OnMouseMove(MSG msg) {
	if (!visible_) return false;
	if (!mousedown_) return false;
	
	int x = GET_X_LPARAM(msg.lParam);
	int y = GET_Y_LPARAM(msg.lParam);
	if (!IsInside(x, y)) return false;

	if (msg.wParam & MK_CONTROL) {
		SelectTarget(InterfaceToWorld(x, y));
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

	GW::Vector2f v = InterfaceToWorld(x, y);
	return pingslines_renderer.OnMouseMove(v.x, v.y);
}

bool Minimap::OnMouseWheel(MSG msg) {
	if (!visible_) return false;

	int x = GET_X_LPARAM(msg.lParam);
	int y = GET_Y_LPARAM(msg.lParam);
	if (!IsInside(x, y)) return false;

	if (!freeze_) {
		int zDelta = GET_WHEEL_DELTA_WPARAM(msg.wParam);
		int delta = zDelta > 0 ? 2 : -2;
		SetWidth(GetWidth() + delta * 2);
		SetHeight(GetHeight() + delta * 2);
		SetX(GetX() - delta);
		SetY(GetY() - delta);

		Config::IniWriteLong(Minimap::IniSection(), Minimap::IniKeyWidth(), GetWidth());

		return true;
	}
	return false;
}
