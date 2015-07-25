/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_EVENT_EVENT_HPP
#define OSHGUI_EVENT_EVENT_HPP

#include <functional>
#include <algorithm>

#include "EventHandler.hpp"
#include "../Exports.hpp"

namespace OSHGui
{
	/**
	 * Event für Funktionen mit einem Parameter.
	 */
	template<typename Signature>
	class OSHGUI_EXPORT Event
	{
	private:
		class EventHandlerInfo
		{
		public:
			EventHandler<Signature> EventHandler;
			bool Remove;
		};

		std::vector<EventHandlerInfo> eventHandlerInfos_;

	public:
		/**
		 * Registriert einen EventHandler.
		 *
		 * \param eventHandler
		 */
		Event& operator+=(const EventHandler<Signature> &eventHandler)
		{
			EventHandlerInfo info = {
				eventHandler,
				false
			};
			eventHandlerInfos_.push_back(info);

			return *this;
		}
		
		/**
		 * Entfernt einen EventHandler.
		 *
		 * \param eventHandler
		 */
		Event& operator-=(const EventHandler<Signature> &eventHandler)
		{
			for (auto &info : eventHandlerInfos_)
			{
				if (info.EventHandler == eventHandler)
				{
					info.Remove = true;
				}
			}

			return *this;
		}

#if (_MSC_VER >= 1800) //VS2013

		/**
		* Ruft alle registrierten EventHandler auf.
		*/
		template <typename ...Params>
		void Invoke(Params&& ...params)
		{
			eventHandlerInfos_.erase(std::remove_if(std::begin(eventHandlerInfos_), std::end(eventHandlerInfos_), [&](const EventHandlerInfo &info)
			{
				if (!info.Remove)
				{
					info.EventHandler.GetHandler()(std::forward<Params>(params)...);
				}
				return info.Remove;
			}), std::end(eventHandlerInfos_));
		}

#else //no variadic template support

		/**
		* Ruft alle registrierten EventHandler auf.
		*/
		void Invoke()
		{
			eventHandlerInfos_.erase(std::remove_if(std::begin(eventHandlerInfos_), std::end(eventHandlerInfos_), [&](const EventHandlerInfo &info)
			{
				if (!info.Remove)
				{
					info.EventHandler.GetHandler()();
				}
				return info.Remove;
			}), std::end(eventHandlerInfos_));
		}
		
		/**
		 * Ruft alle registrierten EventHandler auf.
		 *
		 * \param param1 Funktionsparameter
		 */
		template <typename T>
		void Invoke(T&& param1)
		{
			eventHandlerInfos_.erase(std::remove_if(std::begin(eventHandlerInfos_), std::end(eventHandlerInfos_), [&](const EventHandlerInfo &info)
			{
				if (!info.Remove)
				{
					info.EventHandler.GetHandler()(std::forward<T>(param1));
				}
				return info.Remove;
			}), std::end(eventHandlerInfos_));
		}

		/**
		 * Ruft alle registrierten EventHandler auf.
		 *
		 * \param param1 Funktionsparameter
		 * \param param2 Funktionsparameter
		 */
		template <typename T, typename T2>
		void Invoke(T&& param1, T2&& param2)
		{
			eventHandlerInfos_.erase(std::remove_if(std::begin(eventHandlerInfos_), std::end(eventHandlerInfos_), [&](const EventHandlerInfo &info)
			{
				if (!info.Remove)
				{
					info.EventHandler.GetHandler()(std::forward<T>(param1), std::forward<T2>(param2));
				}
				return info.Remove;
			}), std::end(eventHandlerInfos_));
		}

#endif

	};
}

#endif