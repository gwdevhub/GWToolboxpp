#pragma once

#include "GWAPIMgr.h"

namespace GWAPI {

	class CtoSMgr {
		typedef void(__fastcall *SendCtoGSPacket_t)(DWORD ctogsobj, DWORD size, DWORD* packet);
		friend class GWAPIMgr;
		GWAPIMgr* parent;

		SendCtoGSPacket_t CtoGSPacketSendFunction;
		DWORD GetCtoGSObj();

		CtoSMgr(GWAPIMgr* obj);
	public:

		
		// Send packet that uses only dword parameters, can copypaste most gwa2 sendpackets :D
		void SendPacket(DWORD size, ...);

		// Send a packet with a specific struct alignment, used for more complex packets.
		template <class T>
		void SendPacket(T* packet)
		{
			DWORD size = sizeof(T);
			parent->GameThread->Enqueue(CtoGSPacketSendFunction, GetCtoGSObj(), size, (DWORD*)packet);
		}

	};

}