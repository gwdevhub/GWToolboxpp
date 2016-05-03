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

namespace OSHGui {
	namespace Drawing {
		class Image;

		class OSHGUI_EXPORT Graphics {
		public:
			Graphics(GeometryBuffer &buffer);
			~Graphics();

			void SetClip(const RectangleI &clip);

			void Clear();

			void Rotate(const PointF &pivot, const Vector &angles);

			void DrawLine(const Color &color, const PointI &from, const PointI &to);

			void DrawRectangle(const Color &color, const PointI &origin, const SizeI &size);
			void DrawRectangle(const Color &color, const RectangleI &rectangle);
			void DrawRectangle(const Color &color, int x, int y, int width, int height);
			
			void FillRectangle(const Color &color, int x, int y, int width, int height);
			void FillRectangle(const Color &color, const PointI &origin, const SizeI &size);
			void FillRectangle(const Color &color, const RectangleI &rectangle);

			void FillRectangleGradient(const ColorRectangle &colors, int x, int y, int width, int height);
			void FillRectangleGradient(const ColorRectangle &colors, const PointI &origin, const SizeI &size);
			void FillRectangleGradient(const ColorRectangle &colors, const RectangleI &rectangle);

			void FillCircle(const Color &color, const PointI &origin, int radius);
			void FillCircle(const Color &color, int x, int y, int radius);

			void FillEllipse(const Color &color, const PointI &origin, const SizeI &size);
			void FillEllipse(const Color &color, const RectangleI &region);
			void FillEllipse(const Color &color, int x, int y, int width, int height);

			void DrawString(const Misc::UnicodeString &text, const FontPtr &font, const Color &color, const PointI &origin);
			void DrawString(const Misc::UnicodeString &text, const FontPtr &font, const Color &color, int x, int y);

			void DrawImage(const std::shared_ptr<Image> &image, const ColorRectangle &color, const PointF &origin);
			void DrawImage(const std::shared_ptr<Image> &image, const ColorRectangle &color, const PointI &origin, const RectangleI &clip);
			void DrawImage(const std::shared_ptr<Image> &image, const ColorRectangle &color, const PointF &origin, const RectangleF &clip);
			void DrawImage(const std::shared_ptr<Image> &image, const ColorRectangle &color, const RectangleI &area);
			void DrawImage(const std::shared_ptr<Image> &image, const ColorRectangle &color, const RectangleF &area);
			void DrawImage(const std::shared_ptr<Image> &image, const ColorRectangle &color, const RectangleF &area, const RectangleF &clip);

		private:
			GeometryBuffer &buffer;
		};
	}
}

#endif