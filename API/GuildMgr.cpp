#include "GuildMgr.h"

wchar_t* GWAPI::GuildMgr::GetPlayerGuildAnnouncer()
{
	return MemoryMgr::ReadPtrChain<wchar_t*>(MemoryMgr::GetContextPtr(), 2, 0x3C, 0x278);
}

wchar_t* GWAPI::GuildMgr::GetPlayerGuildAnnouncement()
{
	return MemoryMgr::ReadPtrChain<wchar_t*>(MemoryMgr::GetContextPtr(), 2, 0x3C, 0x78);
}

DWORD GWAPI::GuildMgr::GetPlayerGuildIndex()
{
	return *MemoryMgr::ReadPtrChain<DWORD*>(MemoryMgr::GetContextPtr(), 2, 0x3C, 0x60);
}

GWAPI::GW::GuildArray GWAPI::GuildMgr::GetGuildArray()
{
	return *MemoryMgr::ReadPtrChain<GW::GuildArray*>(MemoryMgr::GetContextPtr(), 2, 0x3C, 0x2F8);
}
