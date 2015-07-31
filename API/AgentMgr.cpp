#include "AgentMgr.h"

GWAPI::AgentMgr::AgentArray GWAPI::AgentMgr::GetAgentArray()
{
	AgentArray agRet = *(AgentArray*)MemoryMgr::agArrayPtr;
	if (agRet.size() == 0) throw 1;
	return agRet;
}

GWAPI::AgentMgr::AgentMgr(GWAPIMgr* obj) : parent(obj)
{
	_ChangeTarget = (ChangeTarget_t)MemoryMgr::ChangeTargetFunction;
}

void GWAPI::AgentMgr::ChangeTarget(Agent* Agent)
{
	parent->GameThread->Enqueue(_ChangeTarget, Agent->Id);
}

GWAPI::AgentMgr::Agent* GWAPI::AgentMgr::AgentArray::GetTarget()
{
	return GetIndex(GetTargetId());
}

GWAPI::AgentMgr::Agent* GWAPI::AgentMgr::AgentArray::GetPlayer()
{
	return GetIndex(GetPlayerId());
}

DWORD GWAPI::AgentMgr::AgentArray::GetTargetId()
{
	return *(DWORD*)MemoryMgr::TargetAgentIDPtr;
}

DWORD GWAPI::AgentMgr::AgentArray::GetPlayerId()
{
	return *(DWORD*)MemoryMgr::PlayerAgentIDPtr;
}
