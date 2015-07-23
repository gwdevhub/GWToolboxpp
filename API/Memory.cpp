#include "Memory.h"



template <typename T>
T GWAPI::CMemory::ReadPtrChain(DWORD pBase, long pOffset1 /*= 0*/, long pOffset2 /*= 0*/, long pOffset3 /*= 0*/, long pOffset4 /*= 0*/, long pOffset5 /*= 0*/)
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

void GWAPI::CMemory::Retour(BYTE *src, BYTE *restore, const int len)
{
	DWORD dwBack;

	VirtualProtect(src, len, PAGE_READWRITE, &dwBack);
	memcpy(src, restore, len);

	restore[0] = 0xE9;
	*(DWORD*)(restore + 1) = (DWORD)(src - restore) - 5;

	VirtualProtect(src, len, dwBack, &dwBack);

	delete[] restore;
}

void * GWAPI::CMemory::Detour(BYTE *src, const BYTE *dst, const int len, BYTE** restore /*= NULL*/)
{
	if (restore){
		*restore = new BYTE[len];
		memcpy(*restore, src, len);
	}


	BYTE *jmp = (BYTE*)malloc(len + 5);
	DWORD dwBack;

	VirtualProtect(src, len, PAGE_READWRITE, &dwBack);

	memcpy(jmp, src, len);
	jmp += len;

	jmp[0] = 0xE9;
	*(DWORD*)(jmp + 1) = (DWORD)(src + len - jmp) - 5;

	src[0] = 0xE9;
	*(DWORD*)(src + 1) = (DWORD)(dst - src) - 5;

	for (int i = 5; i < len; i++)
		src[i] = 0x90;

	VirtualProtect(src, len, dwBack, &dwBack);

	return (jmp - len);
}

bool GWAPI::CMemory::Scan()
{
#define SCAN_START (BYTE*)0x401000
#define SCAN_END (BYTE*)0x900000

	for (BYTE* scan = SCAN_START; scan < SCAN_END; scan++)
	{
		// Agent Array
		const BYTE AgentBaseCode[] = { 0x56, 0x8B, 0xF1, 0x3B, 0xF0, 0x72, 0x04 };
		if (!memcmp(scan, AgentBaseCode, sizeof(AgentBaseCode)))
		{
			agArrayPtr = *(AgentArray**)(scan + 0xC);
			PlayerAgentIDPtr = (DWORD*)(agArrayPtr - 0x54);
			TargetAgentIDPtr = (DWORD*)(agArrayPtr - 0x500);
		}

		// Packet Sender Stuff
		const BYTE CtoGSObjectCode[] = { 0x56, 0x33, 0xF6, 0x3B, 0xCE, 0x74, 0x0E, 0x56, 0x33, 0xD2 };
		if (!memcmp(scan, CtoGSObjectCode, sizeof(CtoGSObjectCode)))
		{
			CtoGSObjectPtr = (DWORD*)scan;
		}

		// Actual function.
		const BYTE CtoGSSendCode[] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x2C, 0x53, 0x56, 0x57, 0x8B, 0xF9, 0x85 };
		if (!memcmp(scan, CtoGSSendCode, sizeof(CtoGSSendCode)))
		{
			CtoGSSendFunction = (SendCtoGSPacket_t)scan;
		}

		// Base pointer, used to get context pointer for game world.
		const BYTE BasePointerLocationCode[] = { 0x8B, 0x42, 0x0C, 0x56, 0x8B, 0x35 };
		if (!memcmp(scan, BasePointerLocationCode, sizeof(BasePointerLocationCode)))
		{
			BasePointerLocation = (BYTE*)(*(DWORD*)(scan + 6));
		}

		// Used for gamethread calls, as well as disable/enable rendering.
		const BYTE EngineCode[] = { 0x53, 0x56, 0xDF, 0xE0, 0xF6, 0xC4, 0x41 };
		if (!memcmp(scan, EngineCode, sizeof(EngineCode)))
		{
			RenderLoopLocation = scan + 0x65;
			GameLoopLocation = RenderLoopLocation - 0x76;
			RenderLoopLocation = GameLoopLocation + 0x5D;
		}

		// For Map IDs
		const BYTE MapIdLocationCode[] = { 0xB0, 0x7F, 0x8D, 0x55 };
		if (!memcmp(scan, MapIdLocationCode, sizeof(MapIdLocationCode))){
			MapIDPtr = *(DWORD**)(scan + 0x46);
		}
	}
	return false;
}
