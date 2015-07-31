#pragma once

#include <Windows.h>
#include <queue>

#include "APIMain.h"

/*
	StoC Packet Handler

	Ripped from the GWLP Dumper
	Credits: ACB, _rusty, et al.
	
	This .h file created by 4D 1
	
	
	Usage:
	struct Pak : public Packet{
		// Struct def here
	};
	
	class PakClass : public PacketHandler{
	public:
		PakClass(Packet* pack){
			m_pak = *(Pak*)pack;
		}
	
		virtual bool HandlePacket();
		virtual DWORD GetHeader(){ return m_pak.Header; }
	private:
		Pak m_pak;
	};
	
	bool PakClass::HandlePacket(){
		// Handler here.
	}
*/

namespace GWAPI{

		class StoCMgr{


			class AutoMutex
			{
			public:
				AutoMutex(HANDLE mutex);
				~AutoMutex();
			private:
				HANDLE m_Mutex;
			};

			class Exception{
				char* msg;
				va_list vl;
				int va_len;
			public:
				const char* getMsg();
				Exception(const char* message, ...);
				void clear();
			};
		public:

#pragma pack(1)
			struct Packet
			{
				DWORD Header;
			};
#pragma pack()

			typedef bool(__fastcall *handler)(Packet*, DWORD);

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
				handler HandlerFunc;
			};


			static HANDLE m_PacketQueueMutex;

			StoCMgr(GWAPIMgr* obj);
			~StoCMgr();

			StoCPacketMetadata* GetLSMetaData();
			StoCPacketMetadata* GetGSMetaData();

			handler SetGSPacket(DWORD Header, handler function);
			handler SetLSPacket(DWORD Header, handler function);

			void RestoreGSPacket(DWORD Header);
			void RestoreLSPacket(DWORD Header);

			handler GetGSHandlerFunc(DWORD Header){
				return m_OrigGSHandler[Header];
			}

			handler GetLSHandlerFunc(DWORD Header){
				return m_OrigLSHandler[Header];
			}

			DWORD GetLSCount();
			DWORD GetGSCount();

			template<class T> void AddHandler(DWORD Header, bool Gameserver);

		private:
			template<class T> static bool __fastcall LSEnqueuePacket(Packet* pack, DWORD unk);
			template<class T> static bool __fastcall GSEnqueuePacket(Packet* pack, DWORD unk);
			static void DisplayError(const char* Format, ...);
			static DWORD WINAPI ProcessPacketThread(LPVOID);

		private:
			friend class GWAPIMgr;
			GWAPIMgr* parent;
			DWORD LSPacketMetadataBase = NULL, GSPacketMetadataBase = NULL;
			StoCPacketMetadata* m_LSPacketMetadata;
			StoCPacketMetadata* m_GSPacketMetadata;
			static handler* m_OrigLSHandler;
			static handler* m_OrigGSHandler;
			static std::queue<PacketHandler*> m_PacketQueue;
			int m_LSPacketCount, m_GSPacketCount;
			//static HANDLE m_PacketQueueMutex;
			HANDLE m_PacketQueueThread;
		};

}