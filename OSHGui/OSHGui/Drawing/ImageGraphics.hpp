/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_IMAGEGRAPHICS_HPP
#define OSHGUI_IMAGEGRAPHICS_HPP

#include "Rectangle.hpp"
#include <vector>

namespace OSHGui
{
	namespace Drawing
	{
		class ColorRectangle;
		class CustomizableImage;

		class OSHGUI_EXPORT ImageGraphics
		{
		public:
			ImageGraphics(CustomizableImage &image);

			/**
			 * Löscht den Inhalt der Textur.
			 */
			void Clear();

			void DrawLine(const ColorRectangle &color, const PointF &from, const PointF &to);
			/**
			 * Füllt das Rechteck.
			 *
			 * \param color
			 * \param origin
			 * \param size
			 */
			void DrawRectangle(const ColorRectangle &color, const PointF &origin, const SizeF &size);
			/**
			 * Füllt das Rechteck.
			 *
			 * \param color
			 * \param rectangle
			 */
			void DrawRectangle(const ColorRectangle &color, const RectangleF &rectangle);
			/**
			 * Füllt die Textur mit einem Farbverlauf.
			 *
			 * \param colors die Eckfarben
			 * \param origin
			 * \param size
			 */
			void FillRectangle(const ColorRectangle &colors, const PointF &origin, const SizeF &size);
			/**
			 * Füllt die Textur mit einem Farbverlauf.
			 *
			 * \param colors die Eckfarben
			 * \param origin
			 * \param size
			 */
			void FillRectangle(const ColorRectangle &colors, const RectangleF &rectangle);
			/**
			 * Füllt den Bereich mit der Farbe, der zwischen den Ecken des Polygons liegt.
			 *
			 * \param vertices
			 */
			void FillPolygon(const std::vector<PointF> &vertices, const ColorRectangle &color);
			/**
			 * Füllt einen Kreis.
			 *
			 * \param color
			 * \param origin
			 * \param radius
			 */
			void FillCircle(const ColorRectangle &color, const PointF &origin, float radius);
			/**
			 * Füllt eine Ellipse mit der entsprechenden Größe um den Mittelpunkt herum.
			 *
			 * \param color
			 * \param origin
			 * \param size
			 */
			void FillEllipse(const ColorRectangle &color, const PointF &origin, const SizeF &size);

			void FillEllipse(const ColorRectangle &color, const RectangleF &region);

		private:
			CustomizableImage &image;
		};
	}
}

#endif