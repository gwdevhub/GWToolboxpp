#include "CtoSMgr.h"

void GWAPI::CtoSMgr::SendPacket(DWORD size, ...)
{
	static DWORD* pak = NULL;

	if (pak != NULL) delete pak;

	pak = new DWORD[size / 4];

	va_list vl;

	va_start(vl, size);
	for (DWORD i = 0; i < size / 4; i++)
	{
		pak[i] = va_arg(vl, DWORD);
	}
	va_end(vl);

	parent->GameThread->Enqueue(CtoGSPacketSendFunction, GetCtoGSObj(), size, pak);
}

GWAPI::CtoSMgr::CtoSMgr(GWAPIMgr* obj) : parent(obj)
{
	CtoGSPacketSendFunction = (SendCtoGSPacket_t)MemoryMgr::CtoGSSendFunction;
}

DWORD GWAPI::CtoSMgr::GetCtoGSObj()
{
	return **(DWORD**)(MemoryMgr::CtoGSObjectPtr - 4);
}
