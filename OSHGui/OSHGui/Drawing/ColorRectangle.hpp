/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_COLORRECTANGLE_HPP
#define OSHGUI_DRAWING_COLORRECTANGLE_HPP

#include "Color.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		/**
		 * Definiert ein Rechteckt, das an allen Ecken verschiedene Farben haben kann.
		 */
		class OSHGUI_EXPORT ColorRectangle
		{
		public:
			/**
			 * Legt die Farbe aller Ecken fest.
			 *
			 * \param color
			 */
			ColorRectangle(Color color);
			/**
			 * Legt die Farbe der beiden oberen und unteren Ecken fest.
			 *
			 * \param top
			 * \param bottom
			 */
			ColorRectangle(Color top, Color bottom);
			/**
			 * Legt die Farben der Ecken fest.
			 *
			 * \param topLeft
			 * \param topRight
			 * \param bottomLeft
			 * \param bottomRight
			 */
			ColorRectangle(Color topLeft, Color topRight, Color bottomLeft, Color bottomRight);

			/**
			 * Prüft, ob das ColorRectangle monochromatisch ist.
			 *
			 * \return true, falls alle Ecken die selbe Farbe haben
			 */
			bool IsMonochromatic() const;

			/**
			 * Ruft ein SubRechteck ab.
			 *
			 * \param left linke Seite (0 - 1)
			 * \param right rechte Seite (0 - 1)
			 * \param top obere Seite (0 - 1)
			 * \param bottom untere Seite (0 - 1)
			 * \return SubRechteck mit berechneten Farben
			 */
			ColorRectangle GetSubRectangle(float left, float right, float top, float bottom) const;

			/**
			 * Ruft die Farbe an einem bestimmten Punkt ab.
			 *
			 * \param x
			 * \param y
			 * \return Farbe an einem bestimmten Punkt
			 */
			Color GetColorAtPoint(float x, float y) const;

			ColorRectangle& operator+=(const ColorRectangle &rhs);
			ColorRectangle& operator*=(const ColorRectangle &rhs);
			ColorRectangle& operator*=(float rhs);

			Color TopLeft;
			Color TopRight;
			Color BottomLeft;
			Color BottomRight;
		};

		const ColorRectangle operator+(const ColorRectangle &lhs, const ColorRectangle &rhs);
		const ColorRectangle operator*(const ColorRectangle &lhs, const ColorRectangle &rhs);
		const ColorRectangle operator*(const ColorRectangle &lhs, float rhs);
	}
}

#endif
