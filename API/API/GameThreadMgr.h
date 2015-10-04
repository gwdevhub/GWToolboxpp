#pragma once


#include "GWAPIMgr.h"

#include <vector>
#include <functional>
#include <mutex>


namespace GWAPI{

	// Shoutouts to DarthTon @ unknowncheats.me for this class.

	class GameThreadMgr{

		friend class GWAPIMgr;

		GWAPIMgr* const parent_;

		bool render_state_;

		Hook hk_game_thread_;

		void RestoreHooks();

		std::vector<std::function<void(void)> > calls_;
		std::vector<std::function<void(void)> > calls_permanent_;
		mutable std::mutex call_vector_mutex_;

		GameThreadMgr(GWAPIMgr* obj);
		~GameThreadMgr();

	public:
		
		// For use only in gameloop hook.
		void __stdcall CallFunctions();

		// Add function to gameloop queue, only use if you know what you're doing.
		template<typename F, typename... ArgTypes>
		void Enqueue(F&& Func, ArgTypes&&... Args)
		{
			std::unique_lock<std::mutex> VecLock(call_vector_mutex_);
			calls_.emplace_back(std::bind(std::forward<F>(Func), std::forward<ArgTypes>(Args)...));
		}
		template<typename F, typename... ArgTypes>
		void AddPermanentCall(F&& Func, ArgTypes&&... Args)
		{
			std::unique_lock<std::mutex> VecLock(call_vector_mutex_);
			calls_permanent_.emplace_back(std::bind(std::forward<F>(Func), std::forward<ArgTypes>(Args)...));
		}


		static void gameLoopHook();
		static void renderHook();
		void ToggleRenderHook();
	};

}

