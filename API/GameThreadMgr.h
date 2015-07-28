#pragma once


#include "APIMain.h"

#include <vector>
#include <functional>
#include <mutex>


namespace GWAPI{

	class GameThreadMgr{

		friend class GWAPIMgr;

		GWAPIMgr* parent;

		BYTE GameLoopRestore[5];

		std::vector<std::function<void(void)> > m_Calls;
		mutable std::mutex m_CallVecMutex;

	public:
		GameThreadMgr(GWAPIMgr* obj);
		~GameThreadMgr();
		// For use only in gameloop hook.
		void __stdcall CallFunctions();

		// Add function to queue.
		template<typename F, typename... ArgTypes>
		void Enqueue(F&& Func, ArgTypes&&... Args)
		{
			std::unique_lock<std::mutex> VecLock(m_CallVecMutex);
			m_Calls.emplace_back(std::bind(std::forward<F>(Func), std::forward<ArgTypes>(Args)...));
		}

		static void gameLoopHook();
		static void renderHook();
		void ToggleRenderHook();
	};

}

