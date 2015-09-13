#pragma once


#include "GWAPIMgr.h"

#include <vector>
#include <functional>
#include <mutex>


namespace GWAPI{

	class GameThreadMgr{

		friend class GWAPIMgr;

		GWAPIMgr* parent;

		bool m_RenderingState;

		Hook hkGameThread;

		std::vector<std::function<void(void)> > m_Calls;
		mutable std::mutex m_CallVecMutex;

		GameThreadMgr(GWAPIMgr* obj);
		~GameThreadMgr();

	public:
		
		// For use only in gameloop hook.
		void __stdcall CallFunctions();

		// Add function to gameloop queue, only use if you know what you're doing.
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

