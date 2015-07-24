#pragma once

#include "APIncludes.h"

#include "Memory.h"

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


	};


	void gameLoopHook();
	void renderHook();

	void ToggleRenderHook();

	extern CallQueue GameThread;
}

