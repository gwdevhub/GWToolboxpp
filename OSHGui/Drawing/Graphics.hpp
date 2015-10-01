/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_GRAPHICS_HPP
#define OSHGUI_GRAPHICS_HPP

#include "GeometryBuffer.hpp"
#include "ColorRectangle.hpp"
#include "Font.hpp"
#include <vector>

namespace OSHGui
{
	namespace Drawing
	{
		class Image;

		class OSHGUI_EXPORT Graphics
		{
		public:
			Graphics(GeometryBuffer &buffer);
			~Graphics();

			/**
			 * Legt die Farbe fest, mit der gezeichnet wird.
			 *
			 * \param color die Farbe
			 */
			void SetClip(const RectangleF &clip);

			/**
			 * Löscht den Inhalt der Textur.
			 */
			void Clear();

			void Rotate(const PointF &pivot, const Vector &angles);

			void DrawLine(const Color &color, const PointF &from, const PointF &to);
			/**
			 * Füllt das Rechteck.
			 *
			 * \param color
			 * \param origin
			 * \param size
			 */
			void DrawRectangle(const Color &color, const PointF &origin, const SizeF &size);
			/**
			 * Füllt das Rechteck.
			 *
			 * \param color
			 * \param rectangle
			 */
			void DrawRectangle(const Color &color, const RectangleF &rectangle);
			/**
			 * Füllt das Rechteck.
			 *
			 * \param color
			 * \param rectangle
			 */
			void DrawRectangle(const Color &color, float x, float y, float width, float height);
			/**
			 * Füllt das Rechteck.
			 *
			 * \param color
			 * \param origin
			 * \param size
			 */
			void FillRectangle(const Color &color, const PointF &origin, const SizeF &size);
			/**
			 * Füllt das Rechteck.
			 *
			 * \param color
			 * \param rectangle
			 */
			void FillRectangle(const Color &color, const RectangleF &rectangle);
			/**
			 * Füllt das Rechteck.
			 *
			 * \param color
			 * \param rectangle
			 */
			void FillRectangle(const Color &color, float x, float y, float width, float height);
			/**
			 * Füllt die Textur mit einem Farbverlauf.
			 *
			 * \param colors die Eckfarben
			 * \param origin
			 * \param size
			 */
			void FillRectangleGradient(const ColorRectangle &colors, const PointF &origin, const SizeF &size);
			/**
			 * Füllt die Textur mit einem Farbverlauf.
			 *
			 * \param colors die Eckfarben
			 * \param origin
			 * \param size
			 */
			void FillRectangleGradient(const ColorRectangle &colors, const RectangleF &rectangle);
			/**
			 * Füllt die Textur mit einem Farbverlauf.
			 *
			 * \param colors die Eckfarben
			 * \param origin
			 * \param size
			 */
			void FillRectangleGradient(const ColorRectangle &colors, float x, float y, float width, float height);
			/**
			 * Füllt den Bereich mit der Farbe, der zwischen den Ecken des Polygons liegt.
			 *
			 * \param vertices
			 */
			void FillPolygon(const std::vector<PointF> &vertices, const Color &color);
			/**
			 * Füllt einen Kreis.
			 *
			 * \param color
			 * \param origin
			 * \param radius
			 */
			void FillCircle(const Color &color, const PointF &origin, float radius);
			/**
			 * Füllt einen Kreis.
			 *
			 * \param color
			 * \param x
			 * \param y
			 * \param radius
			 */
			void FillCircle(const Color &color, float x, float y, float radius);
			/**
			 * Füllt eine Ellipse mit der entsprechenden Größe um den Mittelpunkt herum.
			 *
			 * \param color
			 * \param origin
			 * \param size
			 */
			void FillEllipse(const Color &color, const PointF &origin, const SizeF &size);

			void FillEllipse(const Color &color, const RectangleF &region);

			void FillEllipse(const Color &color, float x, float y, float width, float height);

			void DrawString(const Misc::AnsiString &text, const FontPtr &font, const Color &color, const PointF &origin);

			void DrawString(const Misc::AnsiString &text, const FontPtr &font, const Color &color, float x, float y);

			void DrawImage(const std::shared_ptr<Image> &image, const ColorRectangle &color, const PointF &origin);

			void DrawImage(const std::shared_ptr<Image> &image, const ColorRectangle &color, const PointF &origin, const RectangleF &clip);

			void DrawImage(const std::shared_ptr<Image> &image, const ColorRectangle &color, const RectangleF &area);

			void DrawImage(const std::shared_ptr<Image> &image, const ColorRectangle &color, const RectangleF &area, const RectangleF &clip);

		private:
			GeometryBuffer &buffer;
		};
	}
}

#endif