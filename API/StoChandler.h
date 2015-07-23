#pragma once

#include <Windows.h>
#include <queue>

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

class AutoMutex
{
public:
	AutoMutex(HANDLE mutex)
	{
		m_Mutex = mutex;
		if (WaitForSingleObject(m_Mutex, INFINITE) != WAIT_OBJECT_0)
		{
			MessageBoxA(NULL, "Unable to retrieve mutex!", "Auto Mutex", MB_ICONERROR | MB_OK);
		}
	}
	~AutoMutex()
	{
		ReleaseMutex(m_Mutex);
	}
private:
	HANDLE m_Mutex;
};
class CStoCHandler{
public:
	class Exception{
		char* msg;
		va_list vl;
		int va_len;
	public:
		const char* getMsg(){
			return msg;
		}
		Exception(const char* message, ...){
			va_start(vl, message);
			va_len = _vscprintf(message, vl);
			msg = new char[va_len + 1];
			vsprintf_s(msg, (va_len + 1) * sizeof(char), message, vl);
			va_end(vl);
		}
		void clear(){
			delete[] msg;
		}
	};
private:
	class StoChandler{
	public:
		static HANDLE m_PacketQueueMutex;
		
		StoChandler();
		~StoChandler();

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
	
	CStoCHandler(){}
	void operator=(CStoCHandler const&);
	CStoCHandler(CStoCHandler const&);

public:

	static StoChandler* GetInstance(){
		static StoChandler* m_stocobj = new StoChandler();
		return m_stocobj;
	}

};
template <typename T> static T ReadPtrChain(DWORD pBase, long pOffset1 = 0, long pOffset2 = 0, long pOffset3 = 0, long pOffset4 = 0, long pOffset5 = 0)
{
	DWORD pRead = pBase;
	if (pRead == NULL){ return 0; }

	if (pOffset1){ pRead = *(DWORD*)(pRead + pOffset1); }
	if (pRead == NULL){ return 0; }

	if (pOffset2){ pRead = *(DWORD*)(pRead + pOffset2); }
	if (pRead == NULL){ return 0; }

	if (pOffset3){ pRead = *(DWORD*)(pRead + pOffset3); }
	if (pRead == NULL){ return 0; }

	if (pOffset4){ pRead = *(DWORD*)(pRead + pOffset4); }
	if (pRead == NULL){ return 0; }

	if (pOffset5){ pRead = *(DWORD*)(pRead + pOffset5); }
	if (pRead == NULL){ return 0; }

	return (T)(pRead);
}

std::queue<PacketHandler*> CStoCHandler::StoChandler::m_PacketQueue;
HANDLE CStoCHandler::StoChandler::m_PacketQueueMutex;
handler* CStoCHandler::StoChandler::m_OrigLSHandler = NULL;
handler* CStoCHandler::StoChandler::m_OrigGSHandler = NULL;

CStoCHandler::StoChandler::StoChandler(){
	BYTE* start = (BYTE*)0x00401000;
	BYTE* end = (BYTE*)0x00800000;

	BYTE LSPacketMetadataCode[] = { 0x33, 0xF6, 0x8B, 0x55, 0xF8, 0x8B, 0x45, 0x08, 0x8D, 0x4E, 0x20 };
	BYTE GSPacketMetadataCode[] = { 0x83, 0xEC, 0x08, 0x83, 0xF9, 0x04, 0x8B, 0xC2 };

	while (start++ != end)
	{
		if (!LSPacketMetadataBase && !memcmp(LSPacketMetadataCode, start, sizeof(LSPacketMetadataCode)))
		{
			LSPacketMetadataBase = *(DWORD*)(start - 0x38);
		}
		if (!GSPacketMetadataBase && !memcmp(GSPacketMetadataCode, start, sizeof(GSPacketMetadataCode)))
		{
			GSPacketMetadataBase = *(DWORD*)(start + 0x4E);
		}
		if (LSPacketMetadataBase && GSPacketMetadataBase) break;
	}


	if (LSPacketMetadataBase){
		m_LSPacketMetadata = ReadPtrChain<StoCPacketMetadata*>(*(DWORD*)LSPacketMetadataBase, 0x8, 0x20, 0x14, 0x8, 0x2C);
		m_LSPacketCount = ReadPtrChain<int>(*(DWORD*)LSPacketMetadataBase, 0x8, 0x20, 0x14, 0x8, 0x34);
	}
	else throw Exception("LSPacketMetadataBase not found.");

	if (GSPacketMetadataBase){
		m_GSPacketMetadata = ReadPtrChain<StoCPacketMetadata*>(*(DWORD*)GSPacketMetadataBase, 0x8, 0x2C);
		m_GSPacketCount = ReadPtrChain<int>(*(DWORD*)GSPacketMetadataBase, 0x8, 0x34);
	} 
	else throw Exception("GSPacketMetadataBase not found.");

	m_OrigLSHandler = new handler[m_LSPacketCount];
	m_OrigGSHandler = new handler[m_GSPacketCount];

	m_PacketQueueMutex = CreateMutex(NULL, FALSE, NULL);
	if (!m_PacketQueueMutex) throw Exception("Mutex failed to create.");

	m_PacketQueueThread = CreateThread(0, 0, ProcessPacketThread, 0, 0, 0);
}

CStoCHandler::StoChandler::~StoChandler(){
	for(int i = 0; i < m_LSPacketCount; i++)
		if(m_OrigLSHandler[i] != NULL)
			m_LSPacketMetadata[i].HandlerFunc = m_OrigLSHandler[i];
			
	delete[] m_OrigLSHandler;
			
	for(int i = 0; i < m_GSPacketCount; i++)
		if(m_OrigGSHandler[i] != NULL)
			m_GSPacketMetadata[i].HandlerFunc = m_OrigGSHandler[i];
			
	delete[] m_OrigGSHandler;
			
	TerminateThread(m_PacketQueueThread,0);
}

StoCPacketMetadata* CStoCHandler::StoChandler::GetGSMetaData(){
	return m_GSPacketMetadata;
}

StoCPacketMetadata* CStoCHandler::StoChandler::GetLSMetaData(){
	return m_LSPacketMetadata;
}

handler CStoCHandler::StoChandler::SetGSPacket(DWORD Header, handler function){
	m_OrigGSHandler[Header] = m_GSPacketMetadata[Header].HandlerFunc;
	m_GSPacketMetadata[Header].HandlerFunc = function;
	return m_OrigGSHandler[Header];
}
handler CStoCHandler::StoChandler::SetLSPacket(DWORD Header, handler function){
	m_OrigLSHandler[Header] = m_LSPacketMetadata[Header].HandlerFunc;
	m_LSPacketMetadata[Header].HandlerFunc = function;
	return m_OrigLSHandler[Header];
}

void CStoCHandler::StoChandler::RestoreGSPacket(DWORD Header){
	m_GSPacketMetadata[Header].HandlerFunc = m_OrigGSHandler[Header];
	m_OrigGSHandler[Header] = NULL;
}
void CStoCHandler::StoChandler::RestoreLSPacket(DWORD Header){
	m_LSPacketMetadata[Header].HandlerFunc = m_OrigLSHandler[Header];
	m_OrigLSHandler[Header] = NULL;
}

DWORD CStoCHandler::StoChandler::GetLSCount(){
	return m_LSPacketCount;
}
DWORD CStoCHandler::StoChandler::GetGSCount(){
	return m_GSPacketCount;
}

template<class T> bool __fastcall CStoCHandler::StoChandler::LSEnqueuePacket(Packet* pack, DWORD unk)
{
	AutoMutex mutex(m_PacketQueueMutex);
	m_PacketQueue.push(new T(pack));
	return m_OrigLSHandler[pack->Header](pack, unk);
}

template<class T> bool __fastcall CStoCHandler::StoChandler::GSEnqueuePacket(Packet* pack, DWORD unk)
{
	AutoMutex mutex(m_PacketQueueMutex);
	m_PacketQueue.push(new T(pack));
	return m_OrigGSHandler[pack->Header](pack, unk);
}

template<class T> void CStoCHandler::StoChandler::AddHandler(DWORD Header, bool Gameserver)
{
	if (Gameserver)
	{
		m_OrigGSHandler[Header] = m_GSPacketMetadata[Header].HandlerFunc;
		m_GSPacketMetadata[Header].HandlerFunc = GSEnqueuePacket<T>;
	}
	else
	{
		m_OrigLSHandler[Header] = m_LSPacketMetadata[Header].HandlerFunc;
		m_LSPacketMetadata[Header].HandlerFunc = LSEnqueuePacket<T>;
	}
}

DWORD WINAPI CStoCHandler::StoChandler::ProcessPacketThread(LPVOID)
{
	while (true)
	{
		Sleep(15);
		AutoMutex mutex(m_PacketQueueMutex);
		if (m_PacketQueue.empty())
			continue;
		PacketHandler* pack = m_PacketQueue.front();
		m_PacketQueue.pop();
		if (!pack->HandlePacket())
		{
			DisplayError("Unable to handle packet 0x%04X", pack->GetHeader());
			break;
		}
	}
	return -1;
}

void CStoCHandler::StoChandler::DisplayError(const char* Format, ...)
{
	va_list args;
	va_start(args, Format);
	int sz = _vscprintf(Format, args);
	char* buffer = new char[sz];
	vsprintf_s(buffer, sz, Format, args);
	va_end(args);
	MessageBoxA(0, buffer, "ERROR", MB_ICONERROR | MB_OK);
	delete[] buffer;
}
