#pragma once

#include <Windows.h>
#include <queue>

#include "APIMain.h"

/*
	StoC Packet Handler

	Ripped from the GWLP Dumper
	Credits: ACB, _rusty, et al.
	
	This .h file created by 4D 1
	
	
	Usage (Trade stoc packet as example.:


	struct GS1D7_Traded : public Packet {
		DWORD playerNum;
		//... All members of struct except header, that is handled for you by inheritance.
	}

	handler<GS1D7_Traded*> returnfunc = NULL;

	bool __fastcall TradeHandler(GS1D7_Traded* pak, DWORD unk) // ptr to struct you are using, as well as the other unknown ptr.
	{
		// Do shit with packet :P
		printf("Traded by: %d",pak->playerNum);
		return returnfunc(pak,unk); // Do this or else packet will never be seen by client, potential issues.
	}


	// In some init thread or wherever you want to initialize...

	// SetLSHander for loginserver packet, GS for gameserver packets, you will be mostly uninterested in the loginserver packets.
	returnfunc = GW->StoC->SetGSHandler<GS1D7_Traded>(0x1D7,TradeHandler);
	
*/

namespace GWAPI{

		class StoCMgr{
		public:

#pragma pack(1)
			struct Packet
			{
				DWORD Header;
			};
#pragma pack()
			
			template <class T>
			using handler = bool(__fastcall *)(T*, DWORD);

			class PacketHandler
			{
			public:
				virtual bool HandlePacket(){ return false; }
				virtual DWORD GetHeader(){ return -1; }
			};

			struct StoCPacketMetadata
			{
				DWORD* PacketTemplate;
				int TemplateSize;
				handler<Packet*> HandlerFunc;
			};



			StoCMgr(GWAPIMgr* obj);
			~StoCMgr();

			StoCPacketMetadata* GetLSMetaData();
			StoCPacketMetadata* GetGSMetaData();

			template<class T>
			handler<T*> SetGSHandler(DWORD Header, handler<T*> function){
				m_OrigGSHandler[Header] = m_GSPacketMetadata[Header].HandlerFunc;
				m_GSPacketMetadata[Header].HandlerFunc = function;
				return m_OrigGSHandler[Header];
			}


			template<class T>
			handler<T*> SetLSHandler(DWORD Header, handler<T*> function){
				m_OrigLSHandler[Header] = m_LSPacketMetadata[Header].HandlerFunc;
				m_LSPacketMetadata[Header].HandlerFunc = function;
				return m_OrigLSHandler[Header];
			}

			void RestoreGSHandler(DWORD Header);
			void RestoreLSHandler(DWORD Header);

			template<class T>
			handler<T*> GetGSHandlerFunc(DWORD Header){
				return (handler<T*>)m_OrigGSHandler[Header];
			}

			template<class T>
			handler<T*> GetLSHandlerFunc(DWORD Header){
				return (handler<T*>)m_OrigLSHandler[Header];
			}

			DWORD GetLSCount();
			DWORD GetGSCount();

		private:
			friend class GWAPIMgr;
			GWAPIMgr* parent;
			DWORD LSPacketMetadataBase = NULL, GSPacketMetadataBase = NULL;
			StoCPacketMetadata* m_LSPacketMetadata;
			StoCPacketMetadata* m_GSPacketMetadata;
			static handler<Packet*>* m_OrigLSHandler;
			static handler<Packet*>* m_OrigGSHandler;
			int m_LSPacketCount, m_GSPacketCount;
		};

}