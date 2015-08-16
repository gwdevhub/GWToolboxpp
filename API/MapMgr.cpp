#include "MapMgr.h"


void GWAPI::MapMgr::Travel(DWORD MapID, DWORD District /*= 0*/, DWORD Region /*= 0*/, DWORD Language /*= 0*/)
{
	parent->CtoS->SendPacket(0x18, 0xAB, MapID, Region, District, Language, 1);
}

DWORD GWAPI::MapMgr::GetInstanceTime()
{
	return *MemoryMgr::ReadPtrChain<DWORD*>(MemoryMgr::GetContextPtr(), 2, 0x8, 0x1A8);
}

DWORD GWAPI::MapMgr::GetMapID()
{
	return *(DWORD*)MemoryMgr::MapIDPtr;
}

GWAPI::MapMgr::MapMgr(GWAPIMgr* obj) : parent(obj)
{

}

GwConstants::InstanceType GWAPI::MapMgr::GetInstanceType() {
	return *(GwConstants::InstanceType*)(MemoryMgr::agArrayPtr - 0xF0);
}
