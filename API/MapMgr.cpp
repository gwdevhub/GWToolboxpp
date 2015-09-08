#include "MapMgr.h"

#include "MemoryMgr.h"
#include "CtoSMgr.h"

void GWAPI::MapMgr::Travel(DWORD MapID, DWORD District /*= 0*/, int Region /*= 0*/, DWORD Language /*= 0*/)
{
	static PAB_ZoneMap* pak = new PAB_ZoneMap();

	pak->mapid = MapID;
	pak->district = District;
	pak->region = Region;
	pak->language = Language;
	pak->unk = 0;

	parent->CtoS->SendPacket<PAB_ZoneMap>(pak);
}

DWORD GWAPI::MapMgr::GetInstanceTime()
{
	return *MemoryMgr::ReadPtrChain<DWORD*>(MemoryMgr::GetContextPtr(), 2, 0x8, 0x1AC);
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