#include "MemoryMgr.h"
#include "PatternScanner.h"
#include "..\GWToolbox\logger.h"

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


BYTE* GWAPI::MemoryMgr::MapInfoPtr = NULL;

BYTE* GWAPI::MemoryMgr::DialogFunc = NULL;

bool GWAPI::MemoryMgr::Scan()
{
	PatternScanner scan(0x401000, 0x4FF000);
	LOG("[------------------ API SCAN START ------------------]\n");

		// Agent Array
		agArrayPtr = (BYTE*)scan.FindPattern("\x56\x8B\xF1\x3B\xF0\x72\x04", "xxxxxxx", 0xC);
		if (agArrayPtr)
		{
			LOG("agArrayPtr = %X\n", agArrayPtr);
			agArrayPtr = *(BYTE**)agArrayPtr;
			PlayerAgentIDPtr = (BYTE*)(agArrayPtr - 0x54);
			TargetAgentIDPtr = (BYTE*)(agArrayPtr - 0x500);
		}
		else{
			LOG("agArrayPtr = ERR\n");
			return false;
		}

		// Packet Sender Stuff
		CtoGSObjectPtr = (BYTE*)scan.FindPattern("\x56\x33\xF6\x3B\xCE\x74\x0E\x56\x33\xD2", "xxxxxxxxxx", 0);
		if (CtoGSObjectPtr){
			LOG("CtoGSObjectPtr = %X\n", CtoGSObjectPtr);
		}
		else{
			LOG("CtoGSObjectPtr = ERR\n");
			return false;
		}
		CtoGSSendFunction = (BYTE*)scan.FindPattern("\x55\x8B\xEC\x83\xEC\x2C\x53\x56\x57\x8B\xF9\x85", "xxxxxxxxxxxx", 0);
		if (CtoGSSendFunction){
			LOG("CtoGSSendFunction = %X\n", CtoGSSendFunction);
		}
		else{
			LOG("CtoGSSendFunction = ERR\n");
			return false;
		}

		// Base pointer, used to get context pointer for game world.
		BasePointerLocation = (BYTE*)scan.FindPattern("\x8B\x42\x0C\x56\x8B\x35", "xxxxxx", 0);
		if (BasePointerLocation){
			LOG("BasePointerLocation = %X\n", BasePointerLocation);
			BasePointerLocation = (BYTE*)(*(DWORD*)(BasePointerLocation + 6));
		}
		else{
			LOG("BasePointerLocation = ERR\n");
			return false;
		}

		// Used for gamethread calls, as well as disable/enable rendering.
		RenderLoopLocation = (BYTE*)scan.FindPattern("\x53\x56\xDF\xE0\xF6\xC4\x41", "xxxxxxx", 0);
		if (RenderLoopLocation){
			LOG("RenderLoopLocation = %X\n", RenderLoopLocation);
			RenderLoopLocation = RenderLoopLocation + 0x65;
			GameLoopLocation = RenderLoopLocation - 0x76;
			RenderLoopLocation = GameLoopLocation + 0x5D;
		}
		else{
			LOG("RenderLoopLocation = ERR\n");
			return false;
		}

		// For Map IDs
		MapIDPtr = (BYTE*)scan.FindPattern("\xB0\x7F\x8D\x55", "xxxx", 0);
		if (MapIDPtr){
			LOG("MapIDPtr = %X\n", MapIDPtr);
			MapIDPtr = *(BYTE**)(MapIDPtr + 0x46);
		}
		else{
			LOG("MapIDPtr = ERR\n");
			return false;
		}

		// To write info / Debug as a PM in chat
		WriteChatFunction = (BYTE*)scan.FindPattern("\x55\x8B\xEC\x51\x53\x89\x4D\xFC\x8B\x4D\x08\x56\x57\x8B", "xxxxxxxxxxxxxx", 0);
		if (WriteChatFunction){
			LOG("WriteChatFunction = %X\n", WriteChatFunction);
		}
		else{
			LOG("WriteChatFunction = ERR\n");
			return false;
		}

		// Skill timer to use for exact effect times.
		SkillTimerPtr = (BYTE*)scan.FindPattern("\x85\xC9\x74\x15\x8B\xD6\x2B\xD1\x83\xFA\x64", "xxxxxxxxxxx", 0);
		if (SkillTimerPtr){
			LOG("SkillTimerPtr = %X\n", SkillTimerPtr);
			SkillTimerPtr = *(BYTE**)(SkillTimerPtr - 4);
		}
		else{
			LOG("SkillTimerPtr = ERR\n");
			return false;
		}

		// Skill array.
		SkillArray = (BYTE*)scan.FindPattern("\x8D\x04\xB6\x5E\xC1\xE0\x05\x05", "xxxxxxxx", 0);
		if (SkillArray){
			LOG("SkillArray = %X\n", SkillArray);
			SkillArray = *(BYTE**)(SkillArray + 8);
		}
		else{
			LOG("SkillArray = ERR\n");
			return false;
		}

		// Use Skill Function.
		UseSkillFunction = (BYTE*)scan.FindPattern("\x55\x8B\xEC\x83\xEC\x10\x53\x56\x8B\xD9\x57\x8B\xF2\x89\x5D\xF0", "xxxxxxxxxxxxxxxx", 0);
		if (UseSkillFunction){
			LOG("UseSkillFunction = %X\n", UseSkillFunction);
		}
		else{
			LOG("UseSkillFunction = ERR\n");
			return false;
		}


		PostProcessEffectFunction = (BYTE*)scan.FindPattern("\x55\x8B\xEC\x83\xEC\x10\x89\x4D\xF8\xC7\x45\xFC", "xxxxxxxxxxxx", 0);
		if (PostProcessEffectFunction){
			LOG("PostProcessEffectFunction = %X\n", PostProcessEffectFunction);
		}
		else{
			LOG("PostProcessEffectFunction = ERR\n");
			return false;
		}

		ChangeTargetFunction = (BYTE*)scan.FindPattern("\x33\xC0\x3B\xDA\x0F\x95\xC0\x33", "xxxxxxxx", -0x78);
		if (ChangeTargetFunction){
			LOG("ChangeTargetFunction = %X\n", ChangeTargetFunction);
		}
		else{
			LOG("ChangeTargetFunction = ERR\n");
			return false;
		}

		OpenXunlaiFunction = (BYTE*)scan.FindPattern("\x8B\xF1\x6A\x00\xBA\x12", "xxxxxx", -6);
		if (OpenXunlaiFunction){
			LOG("OpenXunlaiFunction = %X\n", OpenXunlaiFunction);
		}
		else{
			LOG("OpenXunlaiFunction = ERR\n");
			return false;
		}
		XunlaiSession = (BYTE*)scan.FindPattern("\x8D\x14\x76\x8D\x14\x90\x8B\x42\x08\xA8\x01\x75\x41", "xxxxxxxxxxxxx", 0);
		if (XunlaiSession){
			LOG("XunlaiSession = %X\n", XunlaiSession);
			XunlaiSession = *(BYTE**)(XunlaiSession - 4);
		}
		else{
			LOG("XunlaiSession = ERR\n");
			return false;
		}

		MoveFunction = (BYTE*)scan.FindPattern("\xD9\x07\xD8\x5D\xF0\xDF\xE0\xF6\xC4\x01", "xxxxxxxxxx", -0x12);
		if (MoveFunction){
			LOG("MoveFunction = %X\n", MoveFunction);
		}
		else{
			LOG("MoveFunction = ERR\n");
			return false;
		}

		WinHandlePtr = (BYTE*)scan.FindPattern("\x56\x8B\xF1\x85\xC0\x89\x35", "xxxxxxx", 0);
		if (WinHandlePtr){
			LOG("WinHandlePtr = %X\n", WinHandlePtr);
			WinHandlePtr = *(BYTE**)(WinHandlePtr + 7);
		}
		else{
			LOG("WinHandlePtr = ERR\n");
			return false;
		}

		BuyItemFunction = (BYTE*)scan.FindPattern("\x8B\x45\x18\x83\xF8\x10\x76\x17\x68", "xxxxxxxxx", -0x2C);
		if (BuyItemFunction){
			LOG("BuyItemFunction = %X\n", BuyItemFunction);
		}
		else{
			LOG("BuyItemFunction = ERR\n");
			return false;
		}
		SellItemFunction = (BYTE*)scan.FindPattern("\x8B\x4D\x20\x85\xC9\x0F\x85\x8E", "xxxxxxxx", -0x56);
		if (SellItemFunction){
			LOG("SellItemFunction = %X\n", SellItemFunction);
		}
		else{
			LOG("SellItemFunction = ERR\n");
			return false;
		}

		TraderBuyClassHook = (BYTE*)scan.FindPattern("\x81\x7B\x10\x01\x01\x00\x00", "xxxxxxx", -0x42);
		if (TraderBuyClassHook){
			LOG("TraderBuyClassHook = %X\n", TraderBuyClassHook);
		}
		else{
			LOG("TraderBuyClassHook = ERR\n");
			return false;
		}

		TraderSellClassHook = (BYTE*)scan.FindPattern("\x8B\x1F\x83\x3B\x0D", "xxxxx", -0x27);
		if (TraderSellClassHook){
			LOG("TraderSellClassHook = %X\n", TraderSellClassHook);
		}
		else{
			LOG("TraderSellClassHook = ERR\n");
			return false;
		}

		TraderFunction = (BYTE*)scan.FindPattern("\x8B\x45\x18\x8B\x55\x10\x85", "xxxxxxx", -0x48);
		if (TraderFunction){
			LOG("TraderFunction = %X\n", TraderFunction);
		}
		else{
			LOG("TraderFunction = ERR\n");
			return false;
		}

		RequestQuoteFunction = (BYTE*)scan.FindPattern("\x81\xEC\x9C\x00\x00\x00\x53\x56\x8B", "xxxxxxxxx", -3);
		if (RequestQuoteFunction){
			LOG("RequestQuoteFunction = %X\n", RequestQuoteFunction);
		}
		else{
			LOG("RequestQuoteFunction = ERR\n");
			return false;
		}

		CraftitemObj = (BYTE*)scan.FindPattern("\x85\xC0\x59\x74\x3C\x83\x05", "xxxxxxx", 0);
		if (CraftitemObj){
			LOG("CraftitemObj = %X\n", CraftitemObj);
			CraftitemObj = *(BYTE**)(CraftitemObj - 0xE);
		}
		else{
			LOG("CraftitemObj = ERR\n");
			return false;
		}

		MapInfoPtr = (BYTE*)scan.FindPattern("\xC3\x8B\x75\xFC\x8B\x04\xB5", "xxxxxxx", 0);
		if (MapInfoPtr){
			LOG("MapInfoPtr = %X\n", MapInfoPtr);
			MapInfoPtr = *(BYTE**)(MapInfoPtr + 7);
		}
		else{
			LOG("MapInfoPtr = ERR\n");
			return false;
		}


		DialogFunc = (BYTE*)scan.FindPattern("\x55\x8B\xEC\x83\xEC\x28\x53\x56\x57\x8B\xF2\x8B\xD9", "xxxxxxxxxxxxx", -0x28);
		if (DialogFunc){
			LOG("DialogFunc = %X\n", DialogFunc);
		}
		else{
			LOG("DialogFunc = ERR\n");
			return false;
		}

		LOG("[--------- API SCAN COMPLETED SUCESSFULLY ---------]\n");
		return true;
}





