/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_COLORPICKER_HPP
#define OSHGUI_COLORPICKER_HPP

#include <memory>
#include "Control.hpp"
#include "../Drawing/Image.hpp"

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
	class OSHGUI_EXPORT ColorPicker : public Control
	{
	public:
		using Control::SetSize;

		/**
		 * Konstruktor der Klasse.
		 */
		ColorPicker();
		
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
		 * Ruft die Farbe an einem bestimmten Punkt ab.
		 *
		 * \param point
		 * \return color
		 */
		Drawing::Color GetColorAtPoint(const Drawing::PointI &point) const;

		/**
		 * Ruft das ColorChangeEvent für das Steuerelement ab.
		 *
		 * \return colorChangeEvent
		 */
		ColorChangedEvent& GetColorChangedEvent();

	protected:
		virtual void PopulateGeometry() override;

		virtual void OnMouseDown(const MouseMessage &mouse) override;
		virtual void OnMouseUp(const MouseMessage &mouse) override;
		virtual void OnMouseMove(const MouseMessage &mouse) override;

	private:
		static const Drawing::SizeI DefaultSize;

		void CreateGradientTexture();
		void CalculateColorCursorLocation();

		void HandleMouseEvent(const MouseMessage &mouse);
	
		Drawing::Color color_;
		Drawing::PointI colorCursorLocation_;
		Drawing::ImagePtr gradient_;

		ColorChangedEvent colorChangedEvent_;

		bool drag_;
	};
}

#endif