//#define GWAPI_USEDIRECTX
#include "API\APIMain.h"
#include <stdio.h>

using namespace GWAPI;

GWAPIMgr* GW;

// Do all your startup things here instead.
void init(HMODULE hModule){
	AllocConsole();
	FILE* fh;
	freopen_s(&fh, "CONOUT$", "w", stdout);

	GW = GWAPIMgr::GetInstance();

	AgentMgr::AgentArray agents = GW->Agents->GetAgentArray();

	AgentMgr::Agent* player = agents.GetPlayer();


	printf("X: %f Y: %f\n", player->X, player->Y);

	SkillbarMgr::Skillbar sb = GW->Skillbar->GetPlayerSkillbar();

	printf("Skill 1: %d Skill 2: %d Skill 3: %d Skill 4: %d Skill 5: %d Skill 6: %d Skill 7: %d Skill 8: %d\n", 
		sb.Skills[0].SkillId, sb.Skills[1].SkillId, sb.Skills[2].SkillId, sb.Skills[3].SkillId, 
		sb.Skills[4].SkillId, sb.Skills[5].SkillId, sb.Skills[6].SkillId, sb.Skills[7].SkillId);

	EffectMgr::EffectArray effects = GW->Effects->GetPlayerEffectArray();

	for (DWORD i = 0; i < effects.size(); i++)
		printf("Effect ID: %d Duration: %.2f Type: %d\n", effects[i].SkillId, effects[i].Duration, effects[i].EffectType);

	while (1){
		Sleep(100);
		if (GetAsyncKeyState(VK_HOME) & 1){
			GW->Skillbar->UseSkill(6);
		}
		if (GetAsyncKeyState(VK_END) & 1){
			GW->Effects->GetDrunkAf(5,1);
		}
		if (GetAsyncKeyState(VK_INSERT) & 1){
			GW->Map->Travel(133);
		}
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