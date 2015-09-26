#include "CtoSMgr.h"


GWAPI::CtoSMgr::SendCtoGSPacket_t GWAPI::CtoSMgr::gs_send_function_ = NULL;

void GWAPI::CtoSMgr::SendPacket(DWORD size, ...)
{
	DWORD* pak = new DWORD[size / 4];;

	va_list vl;

	va_start(vl, size);
	for (DWORD i = 0; i < size / 4; i++)
	{
		pak[i] = va_arg(vl, DWORD);
	}
	va_end(vl);

	parent_->Gamethread()->Enqueue(packetsendintermediary, GetCtoGSObj(), size, pak);
}

GWAPI::CtoSMgr::CtoSMgr(GWAPIMgr* obj) : parent_(obj)
{
	gs_send_function_ = (SendCtoGSPacket_t)MemoryMgr::CtoGSSendFunction;
}

DWORD GWAPI::CtoSMgr::GetCtoGSObj()
{
	return **(DWORD**)(MemoryMgr::CtoGSObjectPtr - 4);
}

void GWAPI::CtoSMgr::packetsendintermediary(DWORD thisptr, DWORD size, DWORD* packet)
{
	gs_send_function_(thisptr, size, packet);

	delete[] packet;
}
