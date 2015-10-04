/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_EVENT_MOUSEMESSAGE_HPP
#define OSHGUI_EVENT_MOUSEMESSAGE_HPP

#include "../Drawing/Point.hpp"
#include "MouseButton.hpp"

namespace OSHGui
{
	enum MouseState
	{
		/**
		 * Unbekannt
		 */
		Unknown,
		/**
		 * Maus wurde bewegt.
		 */
		Move,
		/**
		 * Maustaste wurde gedrückt.
		 */
		Down,
		/**
		 * Maustaste wurde losgelassen.
		 */
		Up,
		/**
		 * Mausrad wurde gedreht.
		 */
		Scroll
	};

	/**
	 * Mausnachricht
	 */
	class OSHGUI_EXPORT MouseMessage
	{
	public:
		/**
		 * Konstruktor der Klasse
		 */
		MouseMessage()
			: state_(MouseState::Unknown),
			  button_(MouseButton::None),
			  delta_(0)
		{

		}

		MouseMessage(MouseState state, MouseButton button, Drawing::PointI location, int delta)
			: state_(state),
			  button_(button),
			  location_(location),
			  delta_(delta)
		{
			
		}

		MouseState GetState() const
		{
			return state_;
		}

		MouseButton GetButton() const
		{
			return button_;
		}

		const Drawing::PointI& GetLocation() const
		{
			return location_;
		}

		int GetDelta() const
		{
			return delta_;
		}

	private:
		MouseState state_;
		MouseButton button_;
		Drawing::PointI location_;
		int delta_;
	};
}

#endif