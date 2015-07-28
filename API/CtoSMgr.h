#pragma once

#include "APIMain.h"

namespace GWAPI {

	class CtoSMgr {
		typedef void(__fastcall *SendCtoGSPacket_t)(DWORD ctogsobj, DWORD size, DWORD* packet);
		friend class GWAPIMgr;
		GWAPIMgr* parent;

		SendCtoGSPacket_t CtoGSPacketSendFunction;
		DWORD GetCtoGSObj();
	public:

		CtoSMgr(GWAPIMgr* obj);

		void SendPacket(DWORD size, ...);

		template <class T>
		void SendPacket(T* packet);

	};

}