#pragma once

#include "APIncludes.h"
#include "APIMain.h"

#include <vector>
#include <functional>
#include <mutex>


namespace GWAPI{

	class GameThreadMgr{

		GWAPIMgr* parent;

		BYTE GameLoopRestore[5];

		std::vector<std::function<void(void)> > m_Calls;
		mutable std::mutex m_CallVecMutex;

	public:
		GameThreadMgr(GWAPIMgr* obj);
		~GameThreadMgr(){
			DWORD dwProt;
			VirtualProtect(MemoryMgr::GameLoopLocation, 5, PAGE_READWRITE, &dwProt);
			memcpy(MemoryMgr::GameLoopLocation, GameLoopRestore, 5);
			VirtualProtect(MemoryMgr::GameLoopLocation, 5, dwProt, NULL);
		}
		// For use only in gameloop hook.
		void __stdcall CallFunctions();

		// Add function to queue.
		template<typename F, typename... ArgTypes>
		void Enqueue(F&& Func, ArgTypes&&... Args);

		static void gameLoopHook();
		static void renderHook();
		void ToggleRenderHook();
	};

}

