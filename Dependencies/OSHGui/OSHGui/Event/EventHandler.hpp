/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_EVENT_EVENTHANDLER_HPP
#define OSHGUI_EVENT_EVENTHANDLER_HPP

#include <functional>
#include "../Exports.hpp"

namespace OSHGui
{
	/**
	 * Stellt die Methode dar, die ein Ereignis behandelt.
	 */
	template <typename Signature>
	class OSHGUI_EXPORT EventHandler
	{
	private:
		typedef std::function<Signature> Handler;
		Handler handler_;
		int id_;
		
		static unsigned int NextID()
		{
			static unsigned int id = 0;
			++id;
			return id;
		}
		
	public:
		/**
		 * Konstruktor der Klasse.
		 */
		EventHandler()
			: id_(0)
		{

		}

		/**
		 * Konstruktor der Klasse.
		 *
		 * \param handler die gebundene Funktion
		 */
		EventHandler(const Handler &handler)
			: handler_(handler),
			  id_(NextID())
		{
			
		}

		bool operator==(const EventHandler &eventHandler)
		{
			return id_ == eventHandler.id_;
		}
		
		/**
		 * Ruft die gebundene Funktion ab.
		 *
		 * \return handler
		 */
		const Handler& GetHandler() const
		{
			return handler_;
		}
	};
}

#endif