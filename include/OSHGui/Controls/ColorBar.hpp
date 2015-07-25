/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_COLORBAR_HPP
#define OSHGUI_COLORBAR_HPP

#include "Control.hpp"

namespace OSHGui
{
	/**
	 * Tritt ein, wenn sich der Wert der Color-Eigenschaft ändert.
	 */
	typedef Event<void(Control*, const Drawing::Color &color)> ColorChangedEvent;
	typedef EventHandler<void(Control*, const Drawing::Color &color)> ColorChangedEventHandler;

	/**
	 * Wird zum Auswählen einer Farbe verwendet.
	 */
	class OSHGUI_EXPORT ColorBar : public Control
	{
	public:
		using Control::SetSize;

		/**
		 * Konstruktor der Klasse.
		 */
		ColorBar();
		
		/**
		 * Legt die Höhe und Breite des Steuerelements fest.
		 *
		 * \param size
		 */
		virtual void SetSize(const Drawing::SizeI &size) override;
		/**
		 * Legt die ausgewählte Farbe fest.
		 *
		 * \param color die Farbe
		 */
		void SetColor(const Drawing::Color &color);
		/**
		 * Ruft die ausgewählte Farbe ab.
		 *
		 * \return color
		 */
		const Drawing::Color& GetColor() const;
		/**
		 * Ruft das ColorChangeEvent für das Steuerelement ab.
		 *
		 * \return colorChangeEvent
		 */
		ColorChangedEvent& GetColorChangedEvent();
		
	protected:
		static const Drawing::SizeI DefaultSize;
		static const Drawing::SizeI DefaultBarSize;

		void UpdateColor();
		void UpdateBars();

		virtual void PopulateGeometry() override;

		virtual void OnMouseDown(const MouseMessage &mouse) override;
		virtual void OnMouseUp(const MouseMessage &mouse) override;
		virtual void OnMouseMove(const MouseMessage &mouse) override;
		virtual bool OnKeyDown(const KeyboardMessage &keyboard) override;
	
		int barIndex_;
		bool drag_[3];
		Drawing::Color color_;

		std::vector<Drawing::ColorRectangle> bars_;
		std::vector<Drawing::PointI> barSliderLocation_;
		std::vector<Drawing::PointI> barSliderAbsoluteLocation_;

		ColorChangedEvent colorChangedEvent_;
	};
}

#endif