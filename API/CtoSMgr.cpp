#include "CtoSMgr.h"


GWAPI::CtoSMgr::SendCtoGSPacket_t GWAPI::CtoSMgr::CtoGSPacketSendFunction = NULL;

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

	parent->GameThread->Enqueue(packetsendintermediary, GetCtoGSObj(), size, pak);
}

GWAPI::CtoSMgr::CtoSMgr(GWAPIMgr* obj) : parent(obj)
{
	CtoGSPacketSendFunction = (SendCtoGSPacket_t)MemoryMgr::CtoGSSendFunction;
}

DWORD GWAPI::CtoSMgr::GetCtoGSObj()
{
	return **(DWORD**)(MemoryMgr::CtoGSObjectPtr - 4);
}

void GWAPI::CtoSMgr::packetsendintermediary(DWORD thisptr, DWORD size, DWORD* packet)
{
	CtoGSPacketSendFunction(thisptr, size, packet);

	delete[] packet;
}
