#include "CtoSMgr.h"

template <class T>
void GWAPI::CtoSMgr::SendPacket(T* packet)
{
	DWORD size = sizeof(T);

	GameThreadMgr::GetInstance()->Enqueue(CtoGSPacketSendFunction, size, (DWORD*)packet);
}

void GWAPI::CtoSMgr::SendPacket(DWORD size, ...)
{
	DWORD* pak = new DWORD[size / 4];
	va_list vl;

	va_start(vl, size);
	for (DWORD i = 0; i < size / 4; i++)
	{
		pak[i] = va_arg(vl, DWORD);
	}
	va_end(vl);

	GameThreadMgr::GetInstance()->Enqueue(CtoGSPacketSendFunction, size, pak);
	delete[] pak;
}

GWAPI::CtoSMgr::CtoSMgr()
{
	if (MemoryMgr::scanCompleted == false) MemoryMgr::Scan();
	CtoGSPacketSendFunction = (SendCtoGSPacket_t)MemoryMgr::CtoGSSendFunction;
}

DWORD GWAPI::CtoSMgr::GetCtoGSObj()
{
	return *(DWORD*)(MemoryMgr::CtoGSObjectPtr - 4);
}
