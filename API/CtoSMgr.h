#pragma once

namespace GWAPI {

	class CtoSMgr {
		typedef void(__fastcall *SendCtoGSPacket_t)(DWORD ctogsobj, DWORD size, DWORD* packet);

		SendCtoGSPacket_t CtoGSPacketSendFunction;
		DWORD GetCtoGSObj();
	public:

		void SendPacket(DWORD size, ...);

		template <class T>
		void SendPacket(T* packet);

	};

}