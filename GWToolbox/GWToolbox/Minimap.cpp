#include "Minimap.h"

#include <Windows.h>
#include <windowsx.h>
#include <thread>
#include <d3d9.h>
#include <d3dx9math.h>

#include <GWCA\GWCA.h>
#include <GWCA\StoCMgr.h>
#include <GWCA\CameraMgr.h>

#include "Config.h"
#include "GWToolbox.h"

Minimap::Minimap() 
	: range_renderer(RangeRenderer()),
	pmap_renderer(PmapRenderer()),
	agent_renderer(AgentRenderer()),
	dragging_(false),
	freeze_(false),
	loading_(false) {

	GWCA::StoC().AddGameServerEvent<GWCA::StoC_Pak::P391_InstanceLoadFile>(
		[this](GWCA::StoC_Pak::P391_InstanceLoadFile* packet) {
		printf("loading map %d\n", packet->map_fileID);
		pmap_renderer.Invalidate();
		loading_ = false;
		return false;
	});

	GWCA::StoC().AddGameServerEvent<GWCA::StoC_Pak::P406>(
		[&](GWCA::StoC_Pak::P406* pak) {
		loading_ = true;
		return false;
	});

	Config& config = GWToolbox::instance().config();
	int x = config.IniReadLong(Minimap::IniSection(), Minimap::IniKeyX(), 50);
	int y = config.IniReadLong(Minimap::IniSection(), Minimap::IniKeyY(), 50);
	int width = config.IniReadLong(Minimap::IniSection(), Minimap::IniKeyWidth(), 600);
	SetX(x);
	SetY(y);
	SetWidth(width);
	SetHeight(width);

	visible_ = config.IniReadBool(Minimap::IniSection(), Minimap::InikeyShow(), false);

	pmap_renderer.Invalidate();
}

void Minimap::Render(IDirect3DDevice9* device) {
	if (!visible_) return;

	using namespace GWCA;
	if (loading_
		|| !GWCA::Map().IsMapLoaded()
		|| GWCA::Map().GetInstanceType() == GwConstants::InstanceType::Loading
		|| GWCA::Agents().GetPlayerId() == 0) {
		return;
	}

	GW::Agent* me = GWCA::Agents().GetPlayer();
	if (me != nullptr) {
		SetTranslation(-me->X, -me->Y);
	}
	float camera_yaw = GWCA::Camera().GetYaw();
	SetRotation(-camera_yaw + (float)M_PI_2);

	device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);

	D3DXMATRIX identity;
	D3DXMatrixIdentity(&identity);
	device->SetTransform(D3DTS_WORLD, &identity);
	device->SetTransform(D3DTS_VIEW, &identity);
	device->SetTransform(D3DTS_PROJECTION, &identity);

	RenderSetupClipping(device);
	RenderSetupProjection(device);
	
	RenderSetupWorldTransforms(device);
	pmap_renderer.Render(device);	

	D3DXMATRIX scale;
	D3DXMatrixScaling(&scale, scale_, scale_, 1);
	device->SetTransform(D3DTS_VIEW, &scale);
	device->SetTransform(D3DTS_WORLD, &identity);
	range_renderer.Render(device);

	RenderSetupWorldTransforms(device);
	agent_renderer.Render(device);

	pingslines_renderer.Render(device);
}

bool Minimap::OnMouseDown(MSG msg) {
	if (!visible_) return false;
	if (freeze_) return false;
	int x = GET_X_LPARAM(msg.lParam);
	int y = GET_Y_LPARAM(msg.lParam);
	if (x > GetX() && x < GetX() + GetWidth()
		&& y > GetY() && y < GetY() + GetHeight()) {
		drag_start_x_ = x;
		drag_start_y_ = y;
		dragging_ = true;
		return true;
	}
	return false;
}

bool Minimap::OnMouseUp(MSG msg) {
	if (!visible_) return false;
	dragging_ = false;
	Config& config = GWToolbox::instance().config();
	config.IniWriteLong(Minimap::IniSection(), Minimap::IniKeyX(), GetX());
	config.IniWriteLong(Minimap::IniSection(), Minimap::IniKeyY(), GetY());
	return false;
}

bool Minimap::OnMouseMove(MSG msg) {
	if (!visible_) return false;
	if (freeze_) return false;
	int x = GET_X_LPARAM(msg.lParam);
	int y = GET_Y_LPARAM(msg.lParam);
	if (dragging_) {
		int diff_x = x - drag_start_x_;
		int diff_y = y - drag_start_y_;
		SetX(GetX() + diff_x);
		SetY(GetY() + diff_y);
		drag_start_x_ = x;
		drag_start_y_ = y;
		return true;
	}
	return false;
}

bool Minimap::OnMouseWheel(MSG msg) {
	if (!visible_) return false;
	if (freeze_) return false;
	int x = GET_X_LPARAM(msg.lParam);
	int y = GET_Y_LPARAM(msg.lParam);
	if (x > GetX() && x < GetX() + GetWidth()
		&& y > GetY() && y < GetY() + GetHeight()) {
		int zDelta = GET_WHEEL_DELTA_WPARAM(msg.wParam);
		int delta = zDelta > 0 ? 2 : -2;
		SetWidth(GetWidth() + delta * 2);
		SetHeight(GetHeight() + delta * 2);
		SetX(GetX() - delta);
		SetY(GetY() - delta);

		GWToolbox::instance().config().IniWriteLong(
			Minimap::IniSection(), Minimap::IniKeyWidth(), GetWidth());

		return true;
	}
	return false;
}
