#include "AgentMgr.h"


GWAPI::AgentMgr* GWAPI::AgentMgr::GetInstance()
{
	static AgentMgr* inst = new AgentMgr();
	return inst;
}

GWAPI::AgentMgr::AgentArray GWAPI::AgentMgr::GetAgentArray()
{
	AgentArray agRet = *(AgentArray*)MemoryMgr::agArrayPtr;
	if (agRet.size() == 0) throw 1;
	return agRet;
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
