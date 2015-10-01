/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_PROGRESSBAR_HPP
#define OSHGUI_PROGRESSBAR_HPP

#include "Control.hpp"
#include "Panel.hpp"

namespace OSHGui
{
	/**
	 * Stellt ein Statusanzeige-Steuerlement dar.
	 */
	class OSHGUI_EXPORT ProgressBar : public Control
	{
	public:
		/**
		 * Konstruktor der Klasse.
		 */
		ProgressBar();
	
		/**
		 * Legt den minimalen Wert fest.
		 *
		 * \param min
		 */
		void SetMin(int min);
		/**
		 * Gibt den minimalen Wert zurück.
		 *
		 * \return der minimale Wert
		 */
		int GetMin() const;
		/**
		 * Legt den maximalen Wert fest.
		 *
		 * \param max
		 */
		void SetMax(int max);
		/**
		 * Gibt den maximalen Wert zurück.
		 *
		 * \return der maximale Wert
		 */
		int GetMax() const;
		/**
		 * Legt den aktuellen Wert fest.
		 *
		 * \param value
		 */
		void SetValue(int value);
		/**
		 * Ruft den aktuellen Wert ab.
		 *
		 * \return der Wert
		 */
		int GetValue() const;
		/**
		 * Legt die Farbe des Fortschrittsbalken fest.
		 *
		 * \param color
		 */
		void SetBarColor(const Drawing::Color &color);
		/**
		 * Gibt die Farbe des Fortschrittsbalken zurück.
		 *
		 * \return die Farbe
		 */
		const Drawing::Color& GetBarColor() const;

		/**
		 * Überprüft, ob sich der Punkt innerhalb des Steuerelements befindet.
		 *
		 * \param point
		 * \return ja / nein
		 */
		virtual bool Intersect(const Drawing::PointI &point) const override;
		
	protected:
		virtual void PopulateGeometry() override;

	private:
		static const Drawing::SizeI DefaultSize;

		void Adjust();
	
		Drawing::Color barColor_;

		int value_;
		int min_;
		int max_;
	};
}

#endif