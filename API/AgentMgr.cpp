#include "AgentMgr.h"

#include "MemoryMgr.h"
#include "GameThreadMgr.h"
#include "CtoSMgr.h"
#include "MapMgr.h"


BYTE* GWAPI::AgentMgr::DialogLogRet = NULL;
DWORD GWAPI::AgentMgr::LastDialogId = 0;


GWAPI::GW::AgentArray GWAPI::AgentMgr::GetAgentArray()
{
	GW::AgentArray* agRet = (GW::AgentArray*)MemoryMgr::agArrayPtr;
	if (agRet->size() == 0) throw API_EXCEPTION;
	return *agRet;
}

std::vector<GWAPI::GW::Agent*> * GWAPI::AgentMgr::GetParty() {
	std::vector<GW::Agent*>* party = new std::vector<GW::Agent*>(GetPartySize());
	GW::AgentArray agents = GetAgentArray();

	for (size_t i = 0; i < agents.size(); ++i) {
		if (agents[i]->Allegiance == 1
			&& (agents[i]->TypeMap & 0x20000)) {

			party->push_back(agents[i]);
		}
	}

	return party;
}

size_t GWAPI::AgentMgr::GetPartySize() {
	size_t ret = 0;
	for (BYTE i = 0; i < 3; ++i) {
		ret += *MemoryMgr::ReadPtrChain<size_t*>(MemoryMgr::GetContextPtr(), 3, 0x4C, 0x54, 0x0C + 0x10 * i);
	}
	return ret;
}

DWORD GWAPI::AgentMgr::GetDistance(GW::Agent* a, GW::Agent* b) {
	return (DWORD)sqrtl((DWORD)(a->X - b->X) * (DWORD)(a->X - b->X) + (DWORD)(a->Y - b->Y) * (DWORD)(a->Y - b->Y));
}

DWORD GWAPI::AgentMgr::GetSqrDistance(GW::Agent* a, GW::Agent* b) {
	return (DWORD)(a->X - b->X) * (DWORD)(a->X - b->X) + (DWORD)(a->Y - b->Y) * (DWORD)(a->Y - b->Y);
}

GWAPI::AgentMgr::AgentMgr(GWAPIMgr* obj) : parent(obj)
{
	_ChangeTarget = (ChangeTarget_t)MemoryMgr::ChangeTargetFunction;
	_Move = (Move_t)MemoryMgr::MoveFunction;
	DialogLogRet = (BYTE*)MemoryMgr::Detour(MemoryMgr::DialogFunc, (BYTE*)AgentMgr::detourDialogLog, 9, &DialogLogRestore);
}


GWAPI::AgentMgr::~AgentMgr()
{
	MemoryMgr::Retour(MemoryMgr::DialogFunc, DialogLogRestore, 9);
}

void GWAPI::AgentMgr::ChangeTarget(GW::Agent* Agent)
{
	parent->GameThread->Enqueue(_ChangeTarget, Agent->Id);
}

void GWAPI::AgentMgr::Move(float X, float Y, DWORD ZPlane /*= 0*/)
{
	static MovePosition* pos = new MovePosition();

	pos->X = X;
	pos->Y = Y;
	pos->ZPlane = ZPlane;

	parent->GameThread->Enqueue(_Move, pos);
}

void GWAPI::AgentMgr::Dialog(DWORD id)
{
	parent->CtoS->SendPacket(0x8, 0x35, id);
}

GWAPI::GW::PartyMemberArray GWAPI::AgentMgr::GetPartyMemberArray()
{
	return *MemoryMgr::ReadPtrChain<GW::PartyMemberArray*>(MemoryMgr::GetContextPtr(), 3, 0x4C, 0x54, 0x4);
}

bool GWAPI::AgentMgr::GetIsPartyLoaded()
{
	if (parent->Map->GetInstanceType() == GwConstants::InstanceType::Loading) return false;

	GW::PartyMemberArray party = GetPartyMemberArray();
	if (!party.IsValid()) return false;

	for (DWORD i = 0; i < party.size(); i++){
		if (party[i].isLoaded == 0) return false;
	}

	return true;
}

GWAPI::GW::MapAgentArray GWAPI::AgentMgr::GetMapAgentArray()
{
	return *MemoryMgr::ReadPtrChain<GW::MapAgentArray*>(MemoryMgr::GetContextPtr(), 2, 0x2C, 0x7C);
}

GWAPI::GW::Agent* GWAPI::AgentMgr::GetPlayer() {
	GW::AgentArray agents = GetAgentArray();
	if (agents.IsValid()) {
		return GetAgentArray()[GetPlayerId()];
	} else {
		return nullptr;
	}
}

GWAPI::GW::Agent* GWAPI::AgentMgr::GetTarget() {
	GW::AgentArray agents = GetAgentArray();
	if (agents.IsValid()) {
		return GetAgentArray()[GetTargetId()];
	} else {
		return nullptr;
	}
}

void GWAPI::AgentMgr::GoNPC(GW::Agent* Agent, DWORD CallTarget /*= 0*/)
{
	parent->CtoS->SendPacket(0xC, 0x33, Agent->Id, CallTarget);
}

void GWAPI::AgentMgr::GoPlayer(GW::Agent* Agent)
{
	parent->CtoS->SendPacket(0x8, 0x2D, Agent->Id);
}

void GWAPI::AgentMgr::GoSignpost(GW::Agent* Agent, BOOL CallTarget /*= 0*/)
{
	parent->CtoS->SendPacket(0xC, 0x4B, Agent->Id, CallTarget);
}

void GWAPI::AgentMgr::CallTarget(GW::Agent* Agent)
{
	parent->CtoS->SendPacket(0xC, 0x1C, 0xA, Agent->Id);
}

void __declspec(naked) GWAPI::AgentMgr::detourDialogLog()
{
	_asm MOV GWAPI::AgentMgr::LastDialogId, ESI
	_asm JMP GWAPI::AgentMgr::DialogLogRet
}
