#include "Graphics.hpp"
#include "Image.hpp"
#include <algorithm>

namespace OSHGui {
	namespace Drawing {
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Graphics::Graphics(GeometryBuffer &buffer) : buffer(buffer) {
			buffer.SetClippingActive(false);
		}
		//---------------------------------------------------------------------------
		Graphics::~Graphics() {	
		}
		//---------------------------------------------------------------------------
		void Graphics::SetClip(const RectangleI &clip) {
			buffer.SetClippingRegion(clip);
			buffer.SetClippingActive(true);
		}
		//---------------------------------------------------------------------------
		//Runtime Functions
		//---------------------------------------------------------------------------
		void Graphics::Clear() {
			buffer.Reset();
		}
		//---------------------------------------------------------------------------
		void Graphics::Rotate(const PointF &pivot, const Vector &angles) {
			buffer.SetPivot(pivot);
			buffer.SetRotation(Quaternion::EulerAnglesDegrees(angles.x, angles.y, angles.z));
		}
		//---------------------------------------------------------------------------
		void Graphics::DrawLine(const Color &color, const PointI &from, const PointI &to) {
			buffer.SetVertexDrawMode(VertexDrawMode::LineList);
			Vertex vertices[] = {
				{ from.cast<float>(), color },
				{ to.cast<float>(), color }
			};
			buffer.AppendGeometry(vertices, 2);
			buffer.SetVertexDrawMode(VertexDrawMode::TriangleList);
		}
		//---------------------------------------------------------------------------
		void Graphics::DrawRectangle(const Color &color, const PointI &origin, const SizeI &size) {
			DrawRectangle(color, origin.X, origin.Y, size.Width, size.Height);
		}
		//---------------------------------------------------------------------------
		void Graphics::DrawRectangle(const Color &color, const RectangleI &rectangle) {
			DrawRectangle(color, rectangle.GetLeft(), rectangle.GetTop(), rectangle.GetWidth(), rectangle.GetHeight());
		}
		//---------------------------------------------------------------------------
		void Graphics::DrawRectangle(const Color &color, int x, int y, int width, int height) {
			FillRectangle(color, x, y, width, 1);
			FillRectangle(color, x, y, 1, height);
			FillRectangle(color, x + width - 1, y, 1, height);
			FillRectangle(color, x, y + height - 1, width, 1);
		}
		//---------------------------------------------------------------------------
		void Graphics::FillRectangle(const Color &color, int x, int y, int width, int height) {
			FillRectangle(color, RectangleI(x, y, width, height));
		}
		//---------------------------------------------------------------------------
		void Graphics::FillRectangle(const Color &color, const PointI &origin, const SizeI &size) {
			FillRectangle(color, RectangleI(origin, size));
		}
		//---------------------------------------------------------------------------
		void Graphics::FillRectangle(const Color &color, const RectangleI &rectangle) {
			RectangleF rectf = rectangle.cast<float>();
			Vertex vertices[] = {
				{ rectf.GetTopLeft(), color },
				{ rectf.GetTopRight(), color },
				{ rectf.GetBottomLeft(), color },
				{ rectf.GetBottomRight(), color },
				{ rectf.GetBottomLeft(), color },
				{ rectf.GetTopRight(), color }
			};
			buffer.AppendGeometry(vertices, 6);
		}
		//---------------------------------------------------------------------------
		void Graphics::FillRectangleGradient(const ColorRectangle &colors, int x, int y, int width, int height) {
			FillRectangleGradient(colors, RectangleI(x, y, width, height));
		}
		//---------------------------------------------------------------------------
		void Graphics::FillRectangleGradient(const ColorRectangle &colors, const PointI &origin, const SizeI &size) {
			FillRectangleGradient(colors, RectangleI(origin, size));
		}
		//---------------------------------------------------------------------------
		void Graphics::FillRectangleGradient(const ColorRectangle &colors, const RectangleI &rectangle) {
			RectangleF rectf = rectangle.cast<float>();
			Vertex vertices[] = {
				{ rectf.GetTopLeft(), colors.TopLeft },
				{ rectf.GetTopRight(), colors.TopRight },
				{ rectf.GetBottomLeft(), colors.BottomLeft },
				{ rectf.GetBottomRight(), colors.BottomRight },
				{ rectf.GetBottomLeft(), colors.BottomLeft },
				{ rectf.GetTopRight(), colors.TopRight }
			};
			buffer.AppendGeometry(vertices, 6);
		}
		//---------------------------------------------------------------------------
		void Graphics::FillCircle(const Color &color, const PointI &origin, int radius) {
			FillCircle(color, origin.X, origin.Y, radius);
		}
		//---------------------------------------------------------------------------
		void Graphics::FillCircle(const Color &color, int x, int y, int radius) {
			FillEllipse(color, x, y, radius, radius);
		}
		//---------------------------------------------------------------------------
		void Graphics::FillEllipse(const Color &color, const PointI &origin, const SizeI &size) {
			FillEllipse(color, origin.X, origin.Y, size.Width, size.Height);
		}
		//---------------------------------------------------------------------------
		void Graphics::FillEllipse(const Color &color, const RectangleI &region) {
			FillEllipse(color, region.GetLeft(), region.GetTop(), region.GetWidth(), region.GetHeight());
		}
		//---------------------------------------------------------------------------
		void Graphics::FillEllipse(const Color &color, int _x, int _y, int width, int height) {
			int a = width / 2;
			int b = height / 2;
			int xc = _x;
			int yc = _y;
			int x = 0;
			int y = b;
			int a2 = a * a;
			int b2 = b * b;
			int xp = 1;
			int yp = y;

			while (b2 * x < a2 * y) {
				++x;
				if ((b2 * x * x + a2 * (y - 0.5f) * (y - 0.5f) - a2 * b2) >= 0) {
					y--;
				}
				if (x == 1 && y != yp) {
					FillRectangle(color, xc, yc + yp - 1, 1, 1);
					FillRectangle(color, xc, yc - yp, 1, 1);
				}
				if (y != yp) {
					FillRectangle(color, xc - x + 1, yc - yp, 2 * x - 1, 1);
					FillRectangle(color, xc - x + 1, yc + yp, 2 * x - 1, 1);
					yp = y;
					xp = x;
				}

				if (b2 * x >= a2 * y) {
					FillRectangle(color, xc - x, yc - yp, 2 * x + 1, 1);
					FillRectangle(color, xc - x, yc + yp, 2 * x + 1, 1);
				}
			}

			xp = x;
			yp = y;
			int divHeight = 1;

			while (y != 0) {
				y--;
				if ((b2 * (x + 0.5f) * (x + 0.5f) + a2 * y * y - a2 * b2) <= 0) {
					x++;
				}

				if (x != xp) {
					divHeight = yp - y;

					FillRectangle(color, xc - xp, yc - yp, 2 * xp + 1, divHeight);
					FillRectangle(color, xc - xp, yc + y + 1, 2 * xp + 1, divHeight);

					xp = x;
					yp = y;
				}

				if (y == 0) {
					divHeight = yp - y + 1;

					FillRectangle(color, xc - xp, yc - yp, 2 * x + 1, divHeight);
					FillRectangle(color, xc - xp, yc + y, 2 * x + 1, divHeight);
				}
			}
		}

		void Graphics::DrawString(const Misc::UnicodeString &text, const FontPtr &font, const Color &color, const PointI &origin) {
			font->DrawText(buffer, text, origin, nullptr, color);
			buffer.SetActiveTexture(nullptr);
		}

		void Graphics::DrawString(const Misc::UnicodeString &text, const FontPtr &font, const Color &color, int x, int y) {
			DrawString(text, font, color, PointI(x, y));
		}

		void Graphics::DrawImage(const ImagePtr &image, const ColorRectangle &color, const PointF &origin) {
			DrawImage(image, color, RectangleF(origin, image->GetSize().cast<float>()));
		}

		void Graphics::DrawImage(const std::shared_ptr<Image> &image, const ColorRectangle &color, const PointI &origin, const RectangleI &clip) {
			DrawImage(image, color, origin.cast<float>(), clip.cast<float>());
		}

		void Graphics::DrawImage(const std::shared_ptr<Image> &image, const ColorRectangle &color, const PointF &origin, const RectangleF &clip) {
			DrawImage(image, color, RectangleF(origin, image->GetSize().cast<float>()), clip);
		}
		
		void Graphics::DrawImage(const ImagePtr &image, const ColorRectangle &color, const RectangleI &area) {
			DrawImage(image, color, area.cast<float>());
		}

		void Graphics::DrawImage(const ImagePtr &image, const ColorRectangle &color, const RectangleF &area) {
			image->Render(buffer, area, nullptr, color);
			buffer.SetActiveTexture(nullptr);
		}

		void Graphics::DrawImage(const std::shared_ptr<Image> &image, const ColorRectangle &color, const RectangleF &area, const RectangleF &clip) {
			image->Render(buffer, area, &clip.OffsetEx(area.GetLocation()), color);
		}
	}
}
