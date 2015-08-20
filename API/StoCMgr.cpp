#include "StoCMgr.h"

GWAPI::StoCMgr::handler<GWAPI::StoCMgr::Packet*>* GWAPI::StoCMgr::m_OrigLSHandler = NULL;
GWAPI::StoCMgr::handler<GWAPI::StoCMgr::Packet*>* GWAPI::StoCMgr::m_OrigGSHandler = NULL;

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
	else throw 1;

	if (GSPacketMetadataBase){
		m_GSPacketMetadata = *MemoryMgr::ReadPtrChain<StoCPacketMetadata**>(GSPacketMetadataBase, 2, 0x8, 0x2C);
		m_GSPacketCount = *MemoryMgr::ReadPtrChain<int*>(GSPacketMetadataBase, 2, 0x8, 0x34);
	}
	else throw 1;

	m_OrigLSHandler = new handler<Packet*>[m_LSPacketCount];
	m_OrigGSHandler = new handler<Packet*>[m_GSPacketCount];

	for (int i = 0; i < m_LSPacketCount; i++)
		m_OrigLSHandler[i] = NULL;

	for (int i = 0; i < m_GSPacketCount; i++)
		m_OrigGSHandler[i] = NULL;

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

}

GWAPI::StoCMgr::StoCPacketMetadata* GWAPI::StoCMgr::GetGSMetaData(){
	return m_GSPacketMetadata;
}

GWAPI::StoCMgr::StoCPacketMetadata* GWAPI::StoCMgr::GetLSMetaData(){
	return m_LSPacketMetadata;
}

void GWAPI::StoCMgr::RestoreGSHandler(DWORD Header){
	m_GSPacketMetadata[Header].HandlerFunc = m_OrigGSHandler[Header];
	m_OrigGSHandler[Header] = NULL;
}
void GWAPI::StoCMgr::RestoreLSHandler(DWORD Header){
	m_LSPacketMetadata[Header].HandlerFunc = m_OrigLSHandler[Header];
	m_OrigLSHandler[Header] = NULL;
}

DWORD GWAPI::StoCMgr::GetLSCount(){
	return m_LSPacketCount;
}
DWORD GWAPI::StoCMgr::GetGSCount(){
	return m_GSPacketCount;
}
