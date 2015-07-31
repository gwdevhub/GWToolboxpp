#include "StoCMgr.h"


std::queue<GWAPI::StoCMgr::PacketHandler*> GWAPI::StoCMgr::m_PacketQueue;
HANDLE GWAPI::StoCMgr::m_PacketQueueMutex;
GWAPI::StoCMgr::handler* GWAPI::StoCMgr::m_OrigLSHandler = NULL;
GWAPI::StoCMgr::handler* GWAPI::StoCMgr::m_OrigGSHandler = NULL;

GWAPI::StoCMgr::StoCMgr(GWAPIMgr* obj) : parent(obj){
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
		m_LSPacketMetadata = *MemoryMgr::ReadPtrChain<StoCPacketMetadata**>(LSPacketMetadataBase, 5, 0x8, 0x20, 0x14, 0x8, 0x2C);
		m_LSPacketCount = *MemoryMgr::ReadPtrChain<int*>(LSPacketMetadataBase, 5, 0x8, 0x20, 0x14, 0x8, 0x34);
	}
	else throw Exception("LSPacketMetadataBase not found.");

	if (GSPacketMetadataBase){
		m_GSPacketMetadata = *MemoryMgr::ReadPtrChain<StoCPacketMetadata**>(GSPacketMetadataBase, 2, 0x8, 0x2C);
		m_GSPacketCount = *MemoryMgr::ReadPtrChain<int*>(GSPacketMetadataBase, 2, 0x8, 0x34);
	}
	else throw Exception("GSPacketMetadataBase not found.");

	m_OrigLSHandler = new handler[m_LSPacketCount];
	m_OrigGSHandler = new handler[m_GSPacketCount];

	m_PacketQueueMutex = CreateMutex(NULL, FALSE, NULL);
	if (!m_PacketQueueMutex) throw Exception("Mutex failed to create.");

	m_PacketQueueThread = CreateThread(0, 0, ProcessPacketThread, 0, 0, 0);
}

GWAPI::StoCMgr::~StoCMgr(){
	for (int i = 0; i < m_LSPacketCount; i++)
		if (m_OrigLSHandler[i] != NULL)
			m_LSPacketMetadata[i].HandlerFunc = m_OrigLSHandler[i];

	delete[] m_OrigLSHandler;

	for (int i = 0; i < m_GSPacketCount; i++)
		if (m_OrigGSHandler[i] != NULL)
			m_GSPacketMetadata[i].HandlerFunc = m_OrigGSHandler[i];

	delete[] m_OrigGSHandler;

	TerminateThread(m_PacketQueueThread, 0);
}

GWAPI::StoCMgr::StoCPacketMetadata* GWAPI::StoCMgr::GetGSMetaData(){
	return m_GSPacketMetadata;
}

GWAPI::StoCMgr::StoCPacketMetadata* GWAPI::StoCMgr::GetLSMetaData(){
	return m_LSPacketMetadata;
}

GWAPI::StoCMgr::handler GWAPI::StoCMgr::SetGSPacket(DWORD Header, handler function){
	m_OrigGSHandler[Header] = m_GSPacketMetadata[Header].HandlerFunc;
	m_GSPacketMetadata[Header].HandlerFunc = function;
	return m_OrigGSHandler[Header];
}
GWAPI::StoCMgr::handler GWAPI::StoCMgr::SetLSPacket(DWORD Header, handler function){
	m_OrigLSHandler[Header] = m_LSPacketMetadata[Header].HandlerFunc;
	m_LSPacketMetadata[Header].HandlerFunc = function;
	return m_OrigLSHandler[Header];
}

void GWAPI::StoCMgr::RestoreGSPacket(DWORD Header){
	m_GSPacketMetadata[Header].HandlerFunc = m_OrigGSHandler[Header];
	m_OrigGSHandler[Header] = NULL;
}
void GWAPI::StoCMgr::RestoreLSPacket(DWORD Header){
	m_LSPacketMetadata[Header].HandlerFunc = m_OrigLSHandler[Header];
	m_OrigLSHandler[Header] = NULL;
}

DWORD GWAPI::StoCMgr::GetLSCount(){
	return m_LSPacketCount;
}
DWORD GWAPI::StoCMgr::GetGSCount(){
	return m_GSPacketCount;
}

template<class T> bool __fastcall GWAPI::StoCMgr::LSEnqueuePacket(Packet* pack, DWORD unk)
{
	AutoMutex mutex(m_PacketQueueMutex);
	m_PacketQueue.push(new T(pack));
	return m_OrigLSHandler[pack->Header](pack, unk);
}

template<class T> bool __fastcall GWAPI::StoCMgr::GSEnqueuePacket(Packet* pack, DWORD unk)
{
	AutoMutex mutex(m_PacketQueueMutex);
	m_PacketQueue.push(new T(pack));
	return m_OrigGSHandler[pack->Header](pack, unk);
}

template<class T> void GWAPI::StoCMgr::AddHandler(DWORD Header, bool Gameserver)
{
	if (Gameserver)
	{
		m_OrigGSHandler[Header] = m_GSPacketMetadata[Header].HandlerFunc;
		m_GSPacketMetadata[Header].HandlerFunc = GSEnqueuePacket < T > ;
	}
	else
	{
		m_OrigLSHandler[Header] = m_LSPacketMetadata[Header].HandlerFunc;
		m_LSPacketMetadata[Header].HandlerFunc = LSEnqueuePacket < T > ;
	}
}

DWORD WINAPI GWAPI::StoCMgr::ProcessPacketThread(LPVOID)
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

void GWAPI::StoCMgr::DisplayError(const char* Format, ...)
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

GWAPI::StoCMgr::AutoMutex::~AutoMutex()
{
	ReleaseMutex(m_Mutex);
}

GWAPI::StoCMgr::AutoMutex::AutoMutex(HANDLE mutex)
{
	m_Mutex = mutex;
	if (WaitForSingleObject(m_Mutex, INFINITE) != WAIT_OBJECT_0)
	{
		MessageBoxA(NULL, "Unable to retrieve mutex!", "Auto Mutex", MB_ICONERROR | MB_OK);
	}
}

void GWAPI::StoCMgr::Exception::clear()
{
	delete[] msg;
}

GWAPI::StoCMgr::Exception::Exception(const char* message, ...)
{
	va_start(vl, message);
	va_len = _vscprintf(message, vl);
	msg = new char[va_len + 1];
	vsprintf_s(msg, (va_len + 1) * sizeof(char), message, vl);
	va_end(vl);
}

const char* GWAPI::StoCMgr::Exception::getMsg()
{
	return msg;
}
