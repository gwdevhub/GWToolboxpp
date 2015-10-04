/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_TIMER_HPP
#define OSHGUI_TIMER_HPP

#include "Control.hpp"

namespace OSHGui
{
	/**
	 * Tritt auf, wenn das angegebene Intervall verstrichen ist.
	 */
	typedef Event<void(Control*)> TickEvent;
	typedef EventHandler<void(Control*)> TickEventHandler;

	/**
	 * Implementiert einen Zeitgeber, der ein Ereignis in benutzerdefinierten
	 * Intervallen auslöst.
	 */
	class OSHGUI_EXPORT Timer : public Control
	{
	public:
		/**
		 * Konstruktor der Klasse.
		 */
		Timer();
		
		/**
		 * Legt fest, ob das Steuerlement auf Benutzerinteraktionen reagieren kann.
		 *
		 * \param isEnabled
		 */
		virtual void SetEnabled(bool isEnabled) override;
		/**
		 * Legt die Frequenz des Timers in Millisekunden fest.
		 *
		 * \param interval die Frequenz in Millisekunden
		 */
		void SetInterval(int interval);
		/**
		 * Ruft die Frequenz des Timers in Millisekunden ab.
		 *
		 * \return interval
		 */
		int GetInterval() const;
		/**
		 * Ruft das TickEvent für das Steuerelement ab.
		 *
		 * \return tickEvent
		 */
		TickEvent& GetTickEvent();
		
		/**
		 * Startet den Timer. Equivalent zu SetEnabled(true);
		 */
		void Start();
		/**
		 * Hält den Timer an. Equivalent zu SetEnabled(false);
		 */
		void Stop();
		/**
		 * Überprüft, ob sich der Punkt innerhalb des Steuerelements befindet.
		 *
		 * \param point
		 * \return ja / nein
		 */
		virtual bool Intersect(const Drawing::PointI &point) const override;

	protected:
		virtual void InjectTime(const Misc::DateTime &time) override;

	private:
		Misc::TimeSpan interval_;
		Misc::DateTime next_;

		TickEvent tickEvent_;
	};
}

#endif