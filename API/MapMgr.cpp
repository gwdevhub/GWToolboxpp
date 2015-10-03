#include "MapMgr.h"

#include "MemoryMgr.h"
#include "CtoSMgr.h"

void GWAPI::MapMgr::Travel(GwConstants::MapID MapID, DWORD District /*= 0*/, int Region /*= 0*/, DWORD Language /*= 0*/)
{
	static PAB_ZoneMap* pak = new PAB_ZoneMap();

	pak->mapid = static_cast<DWORD>(MapID);
	pak->district = District;
	pak->region = Region;
	pak->language = Language;
	pak->unk = 0;

	parent_->CtoS()->SendPacket<PAB_ZoneMap>(pak);
}

DWORD GWAPI::MapMgr::GetInstanceTime()
{
	return *MemoryMgr::ReadPtrChain<DWORD*>(MemoryMgr::GetContextPtr(), 2, 0x8, 0x1AC);
}

GwConstants::MapID GWAPI::MapMgr::GetMapID()
{
	return static_cast<GwConstants::MapID>(*(DWORD*)MemoryMgr::MapIDPtr);
}

GWAPI::MapMgr::MapMgr(GWAPIMgr* obj) : parent_(obj)
{

}

GwConstants::InstanceType GWAPI::MapMgr::GetInstanceType() {
	return *(GwConstants::InstanceType*)(MemoryMgr::agArrayPtr - 0xF0);
}

GWAPI::GW::MissionMapIconArray GWAPI::MapMgr::GetMissionMapIconArray()
{
	return *MemoryMgr::ReadPtrChain<GW::MissionMapIconArray*>(MemoryMgr::GetContextPtr(), 2, 0x2C, 0x7EC);
}
