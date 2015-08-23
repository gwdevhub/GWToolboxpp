#include "MemoryMgr.h"

// Agent Array
BYTE* GWAPI::MemoryMgr::agArrayPtr = NULL;
BYTE* GWAPI::MemoryMgr::PlayerAgentIDPtr = NULL;
BYTE* GWAPI::MemoryMgr::TargetAgentIDPtr = NULL;

// Map ID
BYTE* GWAPI::MemoryMgr::MapIDPtr = NULL;

// Gameserver PacketSend Addresses
BYTE* GWAPI::MemoryMgr::CtoGSObjectPtr = NULL;
BYTE* GWAPI::MemoryMgr::CtoGSSendFunction = NULL;

// Base ptr to get context pointer, which houses basically
BYTE* GWAPI::MemoryMgr::BasePointerLocation = NULL;

// Renderloop / Main Gameloop
BYTE* GWAPI::MemoryMgr::RenderLoopLocation = NULL;
BYTE* GWAPI::MemoryMgr::GameLoopLocation = NULL;
BYTE* GWAPI::MemoryMgr::GameLoopReturn = NULL;
BYTE* GWAPI::MemoryMgr::GameLoopRestore = NULL;

// Chat function for simple debug/notifications
BYTE* GWAPI::MemoryMgr::WriteChatFunction = NULL;

// Used to get precise skill recharge times.
BYTE* GWAPI::MemoryMgr::SkillTimerPtr = NULL;

BYTE* GWAPI::MemoryMgr::PostProcessEffectFunction = NULL;

// Used to get skill information.
BYTE* GWAPI::MemoryMgr::SkillArray = NULL;
BYTE* GWAPI::MemoryMgr::UseSkillFunction = NULL;

BYTE* GWAPI::MemoryMgr::ChangeTargetFunction = NULL;

BYTE* GWAPI::MemoryMgr::XunlaiSession = NULL;
BYTE* GWAPI::MemoryMgr::OpenXunlaiFunction = NULL;

BYTE* GWAPI::MemoryMgr::MoveFunction = NULL;

BYTE* GWAPI::MemoryMgr::WinHandlePtr = NULL;

// Merchant functions/object pointers
BYTE* GWAPI::MemoryMgr::CraftitemObj = NULL;
BYTE* GWAPI::MemoryMgr::TraderFunction = NULL;
BYTE* GWAPI::MemoryMgr::TraderBuyClassHook = NULL;
BYTE* GWAPI::MemoryMgr::TraderSellClassHook = NULL;
BYTE* GWAPI::MemoryMgr::RequestQuoteFunction = NULL;
BYTE* GWAPI::MemoryMgr::SellItemFunction = NULL;
BYTE* GWAPI::MemoryMgr::BuyItemFunction = NULL;

void GWAPI::MemoryMgr::Retour(BYTE *src, BYTE *restore, const int len)
{
	DWORD dwBack;

	VirtualProtect(src, len, PAGE_READWRITE, &dwBack);
	memcpy(src, restore, len);

	restore[0] = 0xE9;
	*(DWORD*)(restore + 1) = (DWORD)(src - restore) - 5;

	VirtualProtect(src, len, dwBack, &dwBack);

	delete[] restore;
}

void* GWAPI::MemoryMgr::Detour(BYTE *src, const BYTE *dst, const int len, BYTE** restore /*= NULL*/)
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

bool GWAPI::MemoryMgr::Scan()
{
#define SCAN_START (BYTE*)0x401000
#define SCAN_END (BYTE*)0x900000

	for (BYTE* scan = SCAN_START; scan < SCAN_END; scan++)
	{
		// Agent Array
		const BYTE AgentBaseCode[] = { 0x56, 0x8B, 0xF1, 0x3B, 0xF0, 0x72, 0x04 };
		if (!memcmp(scan, AgentBaseCode, sizeof(AgentBaseCode)))
		{
			agArrayPtr = *(BYTE**)(scan + 0xC);
			PlayerAgentIDPtr = (BYTE*)(agArrayPtr - 0x54);
			TargetAgentIDPtr = (BYTE*)(agArrayPtr - 0x500);
		}

		// Packet Sender Stuff
		const BYTE CtoGSObjectCode[] = { 0x56, 0x33, 0xF6, 0x3B, 0xCE, 0x74, 0x0E, 0x56, 0x33, 0xD2 };
		const BYTE CtoGSSendCode[] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x2C, 0x53, 0x56, 0x57, 0x8B, 0xF9, 0x85 };
		if (!memcmp(scan, CtoGSObjectCode, sizeof(CtoGSObjectCode)))
		{
			CtoGSObjectPtr = scan;
		}
		if (!memcmp(scan, CtoGSSendCode, sizeof(CtoGSSendCode)))
		{
			CtoGSSendFunction = scan;
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
			MapIDPtr = *(BYTE**)(scan + 0x46);
		}

		// To write info / Debug as a PM in chat
		const BYTE WriteChatCode[] = { 0x55, 0x8B, 0xEC, 0x51, 0x53, 0x89, 0x4D, 0xFC, 0x8B, 0x4D, 0x08, 0x56, 0x57, 0x8B };
		if (!memcmp(scan, WriteChatCode, sizeof(WriteChatCode))){
			WriteChatFunction = (BYTE*)scan;
		}

		// Skill timer to use for exact effect times.
		const BYTE SkillTimerCode[] = { 0x85, 0xc9, 0x74, 0x15, 0x8b, 0xd6, 0x2b, 0xd1, 0x83, 0xfa, 0x64 };
		if (!memcmp(scan, SkillTimerCode, sizeof(SkillTimerCode))){
			SkillTimerPtr = *(BYTE**)(scan - 4);
		}

		// Skill array.
		const BYTE SkillArrayCode[] = { 0x8D, 0x04, 0xB6, 0x5E, 0xC1, 0xE0, 0x05, 0x05 };
		if (!memcmp(scan, SkillArrayCode, sizeof(SkillArrayCode))){
			SkillArray = *(BYTE**)(scan + 8);
		}

		// Use Skill Function.
		const BYTE UseSkillFunctionCode[] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10, 0x53, 0x56, 0x8B, 0xD9, 0x57, 0x8B, 0xF2, 0x89, 0x5D, 0xF0 };
		if (!memcmp(scan, UseSkillFunctionCode, sizeof(UseSkillFunctionCode))){
			UseSkillFunction = scan;
		}

		const BYTE PostProcessEffectFunctionCode[] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10, 0x89, 0x4D, 0xF8, 0xC7, 0x45, 0xFC };
		if (!memcmp(scan, PostProcessEffectFunctionCode, sizeof(PostProcessEffectFunctionCode))){
			PostProcessEffectFunction = scan;
		}

		const BYTE ChangeTargetCode[] = { 0x33, 0xC0, 0x3B, 0xDA, 0x0F, 0x95, 0xC0, 0x33 };
		if (!memcmp(scan, ChangeTargetCode, sizeof(ChangeTargetCode))){
			ChangeTargetFunction = scan - 0x78;
		}

		const BYTE StorageFunctionCode[] = { 0x8B, 0xF1, 0x6A, 0x00, 0xBA, 0x12 };
		const BYTE StorageSessionCode[] = { 0x8D, 0x14, 0x76, 0x8D, 0x14, 0x90, 0x8B, 0x42, 0x08, 0xA8, 0x01, 0x75, 0x41};
		if (!memcmp(scan, StorageFunctionCode, sizeof(StorageFunctionCode))){
			OpenXunlaiFunction = scan - 6;
		}
		if (!memcmp(scan, StorageSessionCode, sizeof(StorageSessionCode))){
			XunlaiSession = *(BYTE**)(scan - 4);
		}

		const BYTE MoveFunctionCode[] = { 0xD9, 0x07, 0xD8, 0x5D, 0xF0, 0xDF, 0xE0, 0xF6, 0xC4, 0x01 };
		if (!memcmp(scan, MoveFunctionCode, sizeof(MoveFunctionCode))){
			MoveFunction = scan - 0x12;
		}

		const BYTE WinHandleCode[] = { 0x56, 0x8B, 0xF1, 0x85, 0xC0, 0x89, 0x35 };
		if (!memcmp(scan, WinHandleCode, sizeof(WinHandleCode))){
			WinHandlePtr = (BYTE*)(*(DWORD*)(scan + 7));
		}

		const BYTE BuyItemCode[] = { 0x8B, 0x45, 0x18, 0x83, 0xF8, 0x10, 0x76, 0x17, 0x68 };
		const BYTE SellItemCode[] = { 0x8B, 0x4D, 0x20, 0x85, 0xC9, 0x0F, 0x85, 0x8E };
		if (!memcmp(scan, SellItemCode, sizeof(SellItemCode))){
			SellItemFunction = scan - 0x56;
		}
		if (!memcmp(scan, BuyItemCode, sizeof(BuyItemCode))){
			BuyItemFunction = scan - 0x2C;
		}

		const BYTE InitTraderBuyCode[] = { 0x81, 0x7B, 0x10, 0x01, 0x01, 0x00, 0x00 };
		const BYTE InitTraderSellCode[] = { 0x8B, 0x1F, 0x83, 0x3B, 0x0D };
		if (!memcmp(scan, InitTraderBuyCode, sizeof(InitTraderBuyCode)))
		{
			TraderBuyClassHook = scan - 0x42;
		}
		if (!memcmp(scan, InitTraderSellCode, sizeof(InitTraderSellCode)))
		{
			TraderSellClassHook = scan - 0x27;
		}

		const BYTE TraderFunctionCode[] = { 0x8B, 0x45, 0x18, 0x8B, 0x55, 0x10, 0x85 };
		if (!memcmp(scan, TraderFunctionCode, sizeof(TraderFunctionCode))){
			TraderFunction = scan - 0x48;
		}

		const BYTE RequestQuoteCode[] = { 0x81, 0xEC, 0x9C, 0x00, 0x00, 0x00, 0x53, 0x56, 0x8B };
		if (!memcmp(scan, RequestQuoteCode, sizeof(RequestQuoteCode))){
			RequestQuoteFunction = scan - 3;
		}


		const BYTE CraftItemBaseCode[] = { 0x85, 0xC0, 0x59, 0x74, 0x3C, 0x83, 0x05 };
		if (!memcmp(scan, CraftItemBaseCode, sizeof(CraftItemBaseCode)))
		{
			CraftitemObj = *(BYTE**)(scan - 0xE);
		}

		if (agArrayPtr &&
			CtoGSObjectPtr &&
			CtoGSSendFunction &&
			BasePointerLocation &&
			RenderLoopLocation &&
			MapIDPtr &&
			WriteChatFunction &&
			SkillTimerPtr &&
			SkillArray &&
			UseSkillFunction &&
			PostProcessEffectFunction &&
			ChangeTargetFunction &&
			OpenXunlaiFunction &&
			XunlaiSession &&
			MoveFunction &&
			WinHandlePtr &&
			SellItemFunction &&
			BuyItemFunction &&
			TraderFunction &&
			RequestQuoteFunction &&
			CraftitemObj &&
			TraderBuyClassHook &&
			TraderSellClassHook
			) {
				return true;
			}
	}
	return false;
}





