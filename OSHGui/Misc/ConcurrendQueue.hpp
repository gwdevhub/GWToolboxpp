/*
 * OldSchoolHack GUI
 *
 * Copyright (c) 2010-2013 KN4CK3R http://www.oldschoolhack.de
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_MISC_CONCURRENDQUEUE_HPP
#define OSHGUI_MISC_CONCURRENDQUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>

namespace OSHGui
{
	namespace Misc
	{
		template<typename T>
		class ConcurrendQueue
		{
		public:
			void Push(const T &item)
			{
				std::unique_lock<std::mutex> lock(mutex_);
				
				queue_.push(item);
				lock.unlock();
				condition_.notify_one();
			}

			void Push(T&& item)
			{
				std::unique_lock<std::mutex> lock(mutex_);
				
				queue_.push(std::move(item));
				lock.unlock();
				condition_.notify_one();
			}

			T Pop()
			{
				std::unique_lock<std::mutex> lock(mutex_);
				
				while (queue_.empty())
				{
					condition_.wait(lock);
				}
				
				auto item = queue_.front();
				queue_.pop();
				return item;
			}

			void Pop(T &item)
			{
				std::unique_lock<std::mutex> lock(mutex_);
				while (queue_.empty())
				{
					condition_.wait(lock);
				}
				item = queue_.front();
				queue_.pop();
			}

			bool IsEmpty() const
			{
				return queue_.empty();
			}

		private:
			std::queue<T> queue_;
			std::mutex mutex_;
			std::condition_variable condition_;
		};
	}
}

#endif