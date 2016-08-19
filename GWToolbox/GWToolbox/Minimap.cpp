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
	mousedown_(false),
	freeze_(false),
	loading_(false),
	visible_(false) {

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

	Scale(0.0002f);

	SetVisible(config.IniReadBool(Minimap::IniSection(), Minimap::InikeyShow(), false));

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

void Minimap::SafeSelectTarget(int ui_x, int ui_y) {
	__try {
		SelectTarget(ui_x, ui_y);
	} __except (EXCEPT_EXPRESSION_LOOP) {
		LOG("SafeSelectTarget __except body\n");
	}
}

void Minimap::SelectTarget(int ui_x, int ui_y) {
	GWCA::GW::AgentArray agents = GWCA::Agents().GetAgentArray();
	if (!agents.valid()) return;

	GWCA::GW::Agent* me = GWCA::Agents().GetPlayer();
	if (me == nullptr) return;

	float x = 2.0f * ui_x / GetWidth() - 1.0f;
	float y = 2.0f * ui_y / GetHeight() - 1.0f;
	x /= GetScale();
	y /= GetScale();
	
	float camera_yaw = GWCA::Camera().GetYaw();
	float angle = -camera_yaw + (float)M_PI_2;
	float rot_x = x * std::cos(angle) - y * std::sin(angle);
	float rot_y = y * std::cos(angle) + x * std::sin(angle);

	GWCA::GamePos pos(rot_x - GetTranslationX(), -rot_y - GetTranslationY());

	float distance = 600.0f * 600.0f;
	int closest = -1;

	for (size_t i = 0; i < agents.size(); ++i) {
		GWCA::GW::Agent* agent = agents[i];
		if (agent == nullptr) continue;
		if (agent->GetIsDead()) continue;
		if (agent->HP == 0) continue;
		if (!agent->GetIsLivingType()) continue;
		float newDistance = GWCA::Agents().GetSqrDistance(pos, agents[i]->pos);
		if (distance > newDistance) {
			distance = newDistance;
			closest = i;
		}
	}

	if (closest > 0) {
		GWCA::Agents().ChangeTarget(agents[closest]);
	}
}

bool Minimap::OnMouseDown(MSG msg) {
	if (!visible_) return false;

	int x = GET_X_LPARAM(msg.lParam);
	int y = GET_Y_LPARAM(msg.lParam);
	if (!IsInside(x, y)) return false;

	mousedown_ = true;

	if (msg.wParam & MK_CONTROL) {
		SelectTarget(x - GetX(), y - GetY());
		return true;
	}

	if (!freeze_) {
		drag_start_x_ = x;
		drag_start_y_ = y;
		return true;
	}

	return false;
}

bool Minimap::OnMouseDblClick(MSG msg) {
	if (!visible_) return false;

	int x = GET_X_LPARAM(msg.lParam);
	int y = GET_Y_LPARAM(msg.lParam);
	if (!IsInside(x, y)) return false;

	if (msg.wParam & MK_CONTROL) {
		SelectTarget(x - GetX(), y - GetY());
		return true;
	}

	return false;
}

bool Minimap::OnMouseUp(MSG msg) {
	if (!visible_) return false;

	mousedown_ = false;
	
	if (!freeze_) {
		Config& config = GWToolbox::instance().config();
		config.IniWriteLong(Minimap::IniSection(), Minimap::IniKeyX(), GetX());
		config.IniWriteLong(Minimap::IniSection(), Minimap::IniKeyY(), GetY());
	}

	return false;
}

bool Minimap::OnMouseMove(MSG msg) {
	if (!visible_) return false;

	if (!mousedown_) return false;
	
	int x = GET_X_LPARAM(msg.lParam);
	int y = GET_Y_LPARAM(msg.lParam);
	if (!IsInside(x, y)) return false;

	if (msg.wParam & MK_CONTROL) {
		SelectTarget(x - GetX(), y - GetY());
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

	return false;
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

		GWToolbox::instance().config().IniWriteLong(
			Minimap::IniSection(), Minimap::IniKeyWidth(), GetWidth());

		return true;
	}
	return false;
}
