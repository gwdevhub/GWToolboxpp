#include "ImageGraphics.hpp"
#include "CustomizableImage.hpp"
#include "ColorRectangle.hpp"
#include <algorithm>

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		ImageGraphics::ImageGraphics(CustomizableImage &image)
			: image(image)
		{
			
		}
		//---------------------------------------------------------------------------
		//Runtime Functions
		//---------------------------------------------------------------------------
		void ImageGraphics::Clear()
		{
			image.SetRectangle(RectangleF(PointF(0, 0), image.GetSize()), Color::Empty());
		}
		//---------------------------------------------------------------------------
		void ImageGraphics::DrawLine(const ColorRectangle &color, const PointF &from, const PointF &to)
		{
			auto dx = std::abs(to.X - from.X);
			auto dy = std::abs(to.Y - from.Y);
			auto sx = from.X < to.X ? 1 : -1;
			auto sy = from.Y < to.Y ? 1 : -1;
			auto err = dx - dy;

			auto x = from.X;
			auto y = from.Y;
			while (true)
			{
				FillRectangle(color, RectangleF(x, y, 1, 1));
				if (x == to.X && y == to.Y)
				{
					break;
				}
				auto err2 = 2 * err;
				if (err2 > -dy)
				{
					err -= dy;
					x += sx;
				}
				if (err2 < dx)
				{
					err += dx;
					y += sy;
				}
			}
		}
		//---------------------------------------------------------------------------
		void ImageGraphics::DrawRectangle(const ColorRectangle &color, const PointF &origin, const SizeF &size)
		{
			DrawRectangle(color, RectangleF(origin, size));
		}
		//---------------------------------------------------------------------------
		void ImageGraphics::DrawRectangle(const ColorRectangle &color, const RectangleF &rectangle)
		{
			FillRectangle(color.GetSubRectangle(0, 1, 0, 0), rectangle.GetLocation(), SizeF(rectangle.GetWidth(), 1));
			FillRectangle(color.GetSubRectangle(0, 0, 0, 1), rectangle.GetLocation(), SizeF(1, rectangle.GetHeight()));
			FillRectangle(color.GetSubRectangle(1, 1, 0, 1), PointF(rectangle.GetLeft() + rectangle.GetWidth() - 1, rectangle.GetTop()), SizeF(1, rectangle.GetHeight()));
			FillRectangle(color.GetSubRectangle(0, 1, 1, 1), PointF(rectangle.GetLeft(), rectangle.GetTop() + rectangle.GetHeight() - 1), SizeF(rectangle.GetWidth(), 1));
		}
		//---------------------------------------------------------------------------
		void ImageGraphics::FillRectangle(const ColorRectangle &colors, const PointF &origin, const SizeF &size)
		{
			FillRectangle(colors, RectangleF(origin, size));
		}
		//---------------------------------------------------------------------------
		void ImageGraphics::FillRectangle(const ColorRectangle &colors, const RectangleF &rectangle)
		{
			image.SetRectangle(rectangle, colors);
		}
		//---------------------------------------------------------------------------
		void ImageGraphics::FillPolygon(const std::vector<PointF> &vertices, const ColorRectangle &color)
		{
			if (vertices.empty())
			{
				return;
			}

			std::vector<int> nodes;

			auto size = image.GetSize();

			for (int y = 0; y < size.Height; ++y)
			{
				nodes.clear();
				int j = vertices.size() - 1;
				for (int i = 0; i < vertices.size(); ++i)
				{
					if ((vertices[i].Y < y && vertices[j].Y >= y) || (vertices[j].Y < y && vertices[i].Y >= y))
					{
						nodes.push_back((int)(vertices[i].X + (y - vertices[i].Y) / (vertices[j].Y - vertices[i].Y) * (vertices[j].X - vertices[i].X)));
					}
					j = i;
				}

				if (!nodes.empty())
				{
					std::sort(std::begin(nodes), std::end(nodes));

					for (int i = 0; i < nodes.size() - 1; i += 2)
					{
						if (nodes[i] >= size.Width)
						{
							break;
						}
						if (nodes[i + 1] > 0)
						{
							if (nodes[i] < 0)
							{
								nodes[i] = 0;
							}
							if (nodes[i + 1] > size.Width)
							{
								nodes[i + 1] = size.Width;
							}
							if (nodes[i] < nodes[i + 1])
							{
								FillRectangle(color, RectangleF(nodes[i], y, nodes[i + 1] - nodes[i], 1));
							}
						}
					}
				}
			}
		}
		//---------------------------------------------------------------------------
		void ImageGraphics::FillCircle(const ColorRectangle &color, const PointF &origin, float radius)
		{
			FillEllipse(color, origin, SizeF(radius, radius));
		}
		//---------------------------------------------------------------------------
		void ImageGraphics::FillEllipse(const ColorRectangle &color, const PointF &origin, const SizeF &size)
		{
			FillEllipse(color, RectangleF(origin, size));
		}
		//---------------------------------------------------------------------------
		void ImageGraphics::FillEllipse(const ColorRectangle &color, const RectangleF &region)
		{
			int a = region.GetWidth() / 2;
			int b = region.GetHeight() / 2;
			int xc = region.GetLeft();
			int yc = region.GetTop();
			int x = 0;
			int y = b;
			int a2 = a * a;
			int b2 = b * b;
			int xp = 1;
			int yp = y;

			while (b2 * x < a2 * y)
			{
				++x;
				if ((b2 * x * x + a2 * (y - 0.5f) * (y - 0.5f) - a2 * b2) >= 0)
				{
					y--;
				}
				if (x == 1 && y != yp)
				{
					FillRectangle(color, RectangleF(xc, yc + yp - 1, 1, 1));
					FillRectangle(color, RectangleF(xc, yc - yp, 1, 1));
				}
				if (y != yp)
				{
					FillRectangle(color, RectangleF(xc - x + 1, yc - yp, 2 * x - 1, 1));
					FillRectangle(color, RectangleF(xc - x + 1, yc + yp, 2 * x - 1, 1));
					yp = y;
					xp = x;
				}

				if (b2 * x >= a2 * y)
				{
					FillRectangle(color, RectangleF(xc - x, yc - yp, 2 * x + 1, 1));
					FillRectangle(color, RectangleF(xc - x, yc + yp, 2 * x + 1, 1));
				}
			}

			xp = x;
			yp = y;
			int divHeight = 1;

			while (y != 0)
			{
				y--;
				if ((b2 * (x + 0.5f) * (x + 0.5f) + a2 * y * y - a2 * b2) <= 0)
				{
					x++;
				}

				if (x != xp)
				{
					divHeight = yp - y;

					FillRectangle(color, RectangleF(xc - xp, yc - yp, 2 * xp + 1, divHeight));
					FillRectangle(color, RectangleF(xc - xp, yc + y + 1, 2 * xp + 1, divHeight));

					xp = x;
					yp = y;
				}

				if (y == 0)
				{
					divHeight = yp - y + 1;

					FillRectangle(color, RectangleF(xc - xp, yc - yp, 2 * x + 1, divHeight));
					FillRectangle(color, RectangleF(xc - xp, yc + y, 2 * x + 1, divHeight));
				}
			}
		}
		//---------------------------------------------------------------------------
	}
}
