/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_TRACKBAR_HPP
#define OSHGUI_TRACKBAR_HPP

#include "Control.hpp"

namespace OSHGui
{
	/**
	 * Tritt auf, wenn der TrackBar-Schieberegler verschoben wird.
	 */
	typedef Event<void(Control*)> ValueChangedEvent;
	typedef EventHandler<void(Control*)> ValueChangedEventHandler;

	/**
	 * Stellt eine Standardtrackleiste dar.
	 */
	class OSHGUI_EXPORT TrackBar : public Control
	{
	public:
		using Control::SetSize;

		/**
		 * Konstruktor der Klasse.
		 */
		TrackBar();

		/**
		 * Legt die Höhe und Breite des Steuerelements fest.
		 *
		 * \param size
		 */
		virtual void SetSize(const Drawing::SizeI &size) override;
		/**
		 * Legt den minimalen Wert für die Schiebereglerposition auf der TrackBar fest.
		 *
		 * \param minimum
		 */
		void SetMinimum(int minimum);
		/**
		 * Ruft den minimalen Wert für die Schiebereglerposition auf der TrackBar ab.
		 *
		 * \return minimum
		 */
		int GetMinimum() const;
		/**
		 * Legt den maximalen Wert für die Schiebereglerposition auf der TrackBar fest.
		 *
		 * \param maximum
		 */
		void SetMaximum(int maximum);
		/**
		 * Ruft den maximalen Wert für die Schiebereglerposition auf der TrackBar ab.
		 *
		 * \return maximum
		 */
		int GetMaximum() const;
		/**
		 * Legt die Anzahl der Positionen zwischen den Teilstrichen fest.
		 *
		 * \param tickFrequency
		 */
		void SetTickFrequency(int tickFrequency);
		/**
		 * Ruft die Anzahl der Positionen zwischen den Teilstrichen ab
		 *
		 * \return tickFrequency
		 */
		int GetTickFrequency() const;
		/**
		 * Legt den aktuellen Wert fest.
		 *
		 * \param value
		 */
		void SetValue(int value);
		/**
		 * Ruft den aktuellen Wert ab.
		 *
		 * \return der aktuelle Wert
		 */
		int GetValue() const;
		/**
		 * Ruft das ValueChangedEvent für das Steuerelement ab.
		 *
		 * \return valueChangedEvent
		 */
		ValueChangedEvent& GetValueChangedEvent();
		
		/**
		 * Berechnet die absolute Position des Steuerelements.
		 */
		virtual void CalculateAbsoluteLocation() override;

	protected:
		virtual void PopulateGeometry() override;

		virtual void OnMouseDown(const MouseMessage &mouse) override;
		virtual void OnMouseUp(const MouseMessage &mouse) override;
		virtual void OnMouseClick(const MouseMessage &mouse) override;
		virtual void OnMouseMove(const MouseMessage &mouse) override;
		virtual void OnMouseScroll(const MouseMessage &mouse) override;
		virtual bool OnKeyDown(const KeyboardMessage &keyboard) override;
	
	private:
		static const Drawing::SizeI SliderSize;
		static const Drawing::SizeI DefaultSize;
		static const int DefaultTickOffset = 7;

		void SetValueInternal(int value);

		void HandleMouseEvent(const MouseMessage &mouse);

		int minimum_;
		int maximum_;
		int value_;
		int tickFrequency_;
		float pixelsPerTick_;
		bool drag_;
		
		Drawing::PointI sliderLocation_;
		Drawing::PointI sliderAbsoluteLocation_;

		ValueChangedEvent valueChangedEvent_;
	};
}

#endif