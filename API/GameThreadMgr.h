#pragma once

#include "APIncludes.h"

#include "MemoryMgr.h"

#include <vector>
#include <functional>
#include <tuple>
#include <mutex>
#include <memory>


namespace GWAPI{

	class GameThreadMgr{

		BYTE GameLoopRestore[5];

		std::vector<std::function<void(void)> > m_Calls;
		mutable std::mutex m_CallVecMutex;

		GameThreadMgr(){
			if (MemoryMgr::scanCompleted == false) MemoryMgr::Scan();

			memcpy(GameLoopRestore, MemoryMgr::GameLoopLocation, 5);
			MemoryMgr::GameLoopReturn = (BYTE*)MemoryMgr::Detour(MemoryMgr::GameLoopLocation, (BYTE*)gameLoopHook, 5);
		}
		~GameThreadMgr(){
			DWORD dwProt;
			VirtualProtect(MemoryMgr::GameLoopLocation, 5, PAGE_READWRITE, &dwProt);
			memcpy(MemoryMgr::GameLoopLocation,GameLoopRestore, 5);
			VirtualProtect(MemoryMgr::GameLoopLocation, 5, dwProt, NULL);
		}
	public:
		// For use only in gameloop hook.
		void __stdcall CallFunctions();

		// Add function to queue.
		template<typename F, typename... ArgTypes>
		void Enqueue(F&& Func, ArgTypes&&... Args);

		static GameThreadMgr* GetInstance();

		static void gameLoopHook();
		static void renderHook();
		void ToggleRenderHook();
	};

}

