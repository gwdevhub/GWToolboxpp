
#include "API\APIMain.h"

using namespace GWAPI;

// Do all your startup things here instead.
void init(HMODULE hModule){
	MemoryMgr* mems = MemoryMgr::GetInstance();

	if (mems->Scan())
	{
		while (1){

			if (GetAsyncKeyState(VK_HOME) & 1){

				WriteChat(L"Map ID: %d", mems->GetMapID());
				Sleep(2000);

				AgentArray Agents = mems->GetAgentArray();

				Agent* player = Agents.GetPlayer();

				if (player == NULL) return;

				WriteChat(L"Player X: %f Y: %f", player->X, player->Y);

				Sleep(2000);

				Skillbar sb = mems->GetPlayerSkillbar();

				WriteChat(L"Skillbar: %d %d %d %d %d %d %d %d",
					sb.Skills[0].SkillId,
					sb.Skills[1].SkillId,
					sb.Skills[2].SkillId,
					sb.Skills[3].SkillId,
					sb.Skills[4].SkillId,
					sb.Skills[5].SkillId,
					sb.Skills[6].SkillId,
					sb.Skills[7].SkillId);

				Sleep(2000);

				EffectArray PlayerEffects = mems->GetPlayerEffectArray();

				for (int i = 0; i < PlayerEffects.size(); i++)
				{
					WriteChat(L"Effect: EffectID = %d, SkillID = %d, EffectType = %d ", PlayerEffects[i].EffectId, PlayerEffects[i].SkillId, PlayerEffects[i].EffectType);
				}

				Sleep(2000);

				Dialog(0x86);
			}
			Sleep(100);
		}
	}else
	{
		MessageBoxA(0, "Did not find all addresses.", 0, 0);
	}
}

// DLL entry point, not safe to stay in this thread for long.
BOOL WINAPI DllMain(_In_ HMODULE _HDllHandle, _In_ DWORD _Reason, _In_opt_ LPVOID _Reserved){
	if (_Reason == DLL_PROCESS_ATTACH){
		DisableThreadLibraryCalls(_HDllHandle);
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)init, _HDllHandle, 0, 0);
	}
	return TRUE;
}