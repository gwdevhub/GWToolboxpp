#include "PingsLinesRenderer.h"

#include <d3dx9math.h>

#include <GWCA\GWCA.h>
#include <GWCA\StoCMgr.h>

PingsLinesRenderer::PingsLinesRenderer() {
	GWCA::StoC().AddGameServerEvent<GWCA::StoC_Pak::P133>(
		[&](GWCA::StoC_Pak::P133* pak) -> bool {
		// TODO session id check
		if (pak->NumberPts == 1) {
			pings.push_front(new TerrainPing(pak->points[0].x, pak->points[0].y));
		}
		printf("Received ping at %d, %d\n", pak->points[0].x, pak->points[0].y);
		return false;
	});

	GWCA::StoC().AddGameServerEvent<GWCA::StoC_Pak::P041>(
		[&](GWCA::StoC_Pak::P041* pak) -> bool {
		pings.push_front(new AgentPing(pak->agent_id));
		return false;
	});
}

void PingsLinesRenderer::CreatePing(float x, float y) {
	static CtoS_P37* packet = new CtoS_P37();

	short sx = static_cast<short>(std::lroundf(x / 100.0f));
	short sy = static_cast<short>(std::lroundf(y / 100.0f));

	packet->NumberPts = 1;
	packet->session_id = 1;
	packet->points[0].x = sx;
	packet->points[0].y = sy;
	DWORD size = sizeof(CtoS_P37);
	printf("Size 0x%X\n", size);
	GWCA::CtoS().SendPacket<CtoS_P37>(packet);

	pings.push_front(new TerrainPing(sx, sy));
}

void PingsLinesRenderer::Render(IDirect3DDevice9* device) {
	//static clock_t test = TBTimer::init();;
	//if (TBTimer::diff(test) > 1000) {
	//	test = TBTimer::init();
	//	GWCA::GW::Agent* me = GWCA::Agents().GetPlayer();
	//	CreatePing(me->X, me->Y);
	//}

	for (Ping* ping : pings) {
		D3DXMATRIX translate, scale, world;
		D3DXMatrixTranslation(&translate, ping->GetX(), ping->GetY(), 0.0f);
		D3DXMatrixScaling(&scale, 100.0f, 100.0f, 1.0f);
		world = scale * translate;
		device->SetTransform(D3DTS_WORLD, &world);
		ping_circle.Render(device);

		int diff = TBTimer::diff(ping->start);
		bool first_loop = diff < 1000;
		diff = diff % 1000;
		diff *= first_loop ? 2 : 1;

		D3DXMatrixScaling(&scale, diff * ping->GetScale(), diff * ping->GetScale(), 1.0f);
		world = scale * translate;
		device->SetTransform(D3DTS_WORLD, &world);
		ping_circle.Render(device);
	}
	if (!pings.empty() && TBTimer::diff(pings.back()->start) > 3000) {
		delete pings.back();
		pings.pop_back();
	}
}

void PingsLinesRenderer::PingCircle::Initialize(IDirect3DDevice9* device) {
	type_ = D3DPT_TRIANGLESTRIP;
	count_ = 48; // polycount
	vertex_count = count_ + 2;
	vertices = nullptr;

	if (buffer_) buffer_->Release();
	device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer_, NULL);
	buffer_->Lock(0, sizeof(D3DVertex) * vertex_count,
		(VOID**)&vertices, D3DLOCK_DISCARD);
	
	for (size_t i = 0; i < count_; ++i) {
		float angle = i * (static_cast<float>(2 * M_PI) / count_);
		bool outer = (i % 2 == 0);
		float radius = outer ? 1.0f : 0.8f;
		vertices[i].x = radius * std::cos(angle);
		vertices[i].y = radius * std::sin(angle);
		vertices[i].z = 0.0f;
		vertices[i].color = D3DCOLOR_ARGB(outer ? 150 : 0, 255, 0, 0);
	}
	vertices[count_] = vertices[0];
	vertices[count_ + 1] = vertices[1];

	buffer_->Unlock();
}

float PingsLinesRenderer::AgentPing::GetX() {
	GWCA::GW::Agent* agent = GWCA::Agents().GetAgentByID(id);
	if (agent == nullptr) return 0.0f;
	return agent->X;
}

float PingsLinesRenderer::AgentPing::GetY() {
	GWCA::GW::Agent* agent = GWCA::Agents().GetAgentByID(id);
	if (agent == nullptr) return 0.0f;
	return agent->Y;
}
