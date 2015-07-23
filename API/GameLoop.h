#pragma once

#include "APIMain.h"


#include <vector>
#include <functional>
#include <tuple>
#include <mutex>
#include <memory>


namespace GWAPI{

	/*
		CallQueue
		Majority of code by DarthTon @ unknowncheats.me
	*/
	class CallQueue{
		std::vector<std::function<void(void)> > m_Calls;
		mutable std::mutex m_CallVecMutex;
	public:
		// For use only in gameloop hook.
		void __stdcall CallFunctions();

		// Add function to queue.
		template<typename F, typename... ArgTypes>
		void Enqueue(F&& Func, ArgTypes&&... Args);


	}GameThread;


	void __declspec(naked) gameLoopHook();
	void __declspec(naked) renderHook();

	void ToggleRenderHook()
	{
		static bool enabled = false;
		static BYTE restorebuf[5];
		static DWORD dwProt;

		enabled = !enabled;

		if (enabled)
		{
			memcpy(restorebuf, Memory.RenderLoopLocation, 5);

			VirtualProtect(Memory.RenderLoopLocation, 5, PAGE_EXECUTE_READWRITE, &dwProt);
			Memory.RenderLoopLocation[0] = 0xE9;
			*(DWORD*)(Memory.RenderLoopLocation) = (DWORD)((BYTE*)renderHook - (Memory.RenderLoopLocation + 5));
			VirtualProtect(Memory.RenderLoopLocation, 5, dwProt, NULL);
		}
		else{
			VirtualProtect(Memory.RenderLoopLocation, 5, PAGE_EXECUTE_READWRITE, &dwProt);
			memcpy(Memory.RenderLoopLocation,restorebuf, 5);
			VirtualProtect(Memory.RenderLoopLocation, 5, dwProt, NULL);
		}
	}
}