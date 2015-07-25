#include "Graphics.hpp"
#include "Image.hpp"
#include <algorithm>

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Graphics::Graphics(GeometryBuffer &buffer)
			: buffer(buffer)
		{
			buffer.SetClippingActive(false);
		}
		//---------------------------------------------------------------------------
		Graphics::~Graphics()
		{
			
		}
		//---------------------------------------------------------------------------
		void Graphics::SetClip(const RectangleF &clip)
		{
			buffer.SetClippingRegion(clip);
			buffer.SetClippingActive(true);
		}
		//---------------------------------------------------------------------------
		//Runtime Functions
		//---------------------------------------------------------------------------
		void Graphics::Clear()
		{
			buffer.Reset();
		}
		//---------------------------------------------------------------------------
		void Graphics::Rotate(const PointF &pivot, const Vector &angles)
		{
			buffer.SetPivot(Vector(pivot.X, pivot.Y, 0.0f));
			buffer.SetRotation(Quaternion::EulerAnglesDegrees(angles.x, angles.y, angles.z));
		}
		//---------------------------------------------------------------------------
		void Graphics::DrawLine(const Color &color, const PointF &from, const PointF &to)
		{
			buffer.SetVertexDrawMode(VertexDrawMode::LineList);

			Vertex vertices[] = {
				{ Vector(from.X, from.Y, 0.0f), color },
				{ Vector(to.X, to.Y, 0.0f), color }
			};
			buffer.AppendGeometry(vertices, 2);

			buffer.SetVertexDrawMode(VertexDrawMode::TriangleList);
		}
		//---------------------------------------------------------------------------
		void Graphics::DrawRectangle(const Color &color, const PointF &origin, const SizeF &size)
		{
			DrawRectangle(color, origin.X, origin.Y, size.Width, size.Height);
		}
		//---------------------------------------------------------------------------
		void Graphics::DrawRectangle(const Color &color, const RectangleF &rectangle)
		{
			DrawRectangle(color, rectangle.GetLeft(), rectangle.GetTop(), rectangle.GetWidth(), rectangle.GetHeight());
		}
		//---------------------------------------------------------------------------
		void Graphics::DrawRectangle(const Color &color, float x, float y, float width, float height)
		{
			FillRectangle(color, x, y, width, 1);
			FillRectangle(color, x, y, 1, height);
			FillRectangle(color, x + width - 1, y, 1, height);
			FillRectangle(color, x, y + height - 1, width, 1);
		}
		//---------------------------------------------------------------------------
		void Graphics::FillRectangle(const Color &color, const PointF &origin, const SizeF &size)
		{
			FillRectangle(color, origin.X, origin.Y, size.Width, size.Height);
		}
		//---------------------------------------------------------------------------
		void Graphics::FillRectangle(const Color &color, const RectangleF &rectangle)
		{
			FillRectangle(color, rectangle.GetLeft(), rectangle.GetTop(), rectangle.GetWidth(), rectangle.GetHeight());
		}
		//---------------------------------------------------------------------------
		void Graphics::FillRectangle(const Color &color, float x, float y, float width, float height)
		{
			Vertex vertices[] = {
				{ Vector(x, y, 0.0f), color },
				{ Vector(x + width, y, 0.0f), color },
				{ Vector(x, y + height, 0.0f), color },
				{ Vector(x + width, y + height, 0.0f), color },
				{ Vector(x, y + height, 0.0f), color },
				{ Vector(x + width, y, 0.0f), color }
			};
			buffer.AppendGeometry(vertices, 6);
		}
		//---------------------------------------------------------------------------
		void Graphics::FillRectangleGradient(const ColorRectangle &colors, const PointF &origin, const SizeF &size)
		{
			FillRectangleGradient(colors, origin.X, origin.Y, size.Width, size.Height);
		}
		//---------------------------------------------------------------------------
		void Graphics::FillRectangleGradient(const ColorRectangle &colors, const RectangleF &rectangle)
		{
			FillRectangleGradient(colors, rectangle.GetLeft(), rectangle.GetTop(), rectangle.GetWidth(), rectangle.GetHeight());
		}
		//---------------------------------------------------------------------------
		void Graphics::FillRectangleGradient(const ColorRectangle &colors, float x, float y, float width, float height)
		{
			Vertex vertices[] = {
				{ Vector(x, y, 0.0f), colors.TopLeft },
				{ Vector(x + width, y, 0.0f), colors.TopRight },
				{ Vector(x, y + height, 0.0f), colors.BottomLeft },
				{ Vector(x + width, y + height, 0.0f), colors.BottomRight },
				{ Vector(x, y + height, 0.0f), colors.BottomLeft },
				{ Vector(x + width, y, 0.0f), colors.TopRight }
			};
			buffer.AppendGeometry(vertices, 6);
		}
		//---------------------------------------------------------------------------
		void Graphics::FillPolygon(const std::vector<PointF> &vertices, const Color &color)
		{
			if (vertices.empty())
			{
				return;
			}

			std::vector<int> nodes;

			/*auto size = texture->GetSize();

			for (int y = 0; y < size.Height; ++y)
			{
				nodes.clear();
				int j = vertices.size() - 1;
				for (int i = 0; i < vertices.size(); ++i)
				{
					if ((vertices[i].Y < y && vertices[j].Y >= y) || (vertices[j].Y < y && vertices[i].Y >= y))
					{
						nodes.push_back((int)(vertices[i].X + (y - vertices[i].Y) / (float)(vertices[j].Y - vertices[i].Y) * (vertices[j].X - vertices[i].X)));
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
								//texture->Fill(nodes[i], y, nodes[i + 1] - nodes[i], 1, color);
							}
						}
					}
				}
			}*/
		}
		//---------------------------------------------------------------------------
		void Graphics::FillCircle(const Color &color, const PointF &origin, float radius)
		{
			FillCircle(color, origin.X, origin.Y, radius);
		}
		//---------------------------------------------------------------------------
		void Graphics::FillCircle(const Color &color, float x, float y, float radius)
		{
			FillEllipse(color, x, y, radius, radius);
		}
		//---------------------------------------------------------------------------
		void Graphics::FillEllipse(const Color &color, const PointF &origin, const SizeF &size)
		{
			FillEllipse(color, origin.X, origin.Y, size.Width, size.Height);
		}
		//---------------------------------------------------------------------------
		void Graphics::FillEllipse(const Color &color, const RectangleF &region)
		{
			FillEllipse(color, region.GetLeft(), region.GetTop(), region.GetWidth(), region.GetHeight());
		}
		//---------------------------------------------------------------------------
		void Graphics::FillEllipse(const Color &color, float _x, float _y, float width, float height)
		{
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

			while (b2 * x < a2 * y)
			{
				++x;
				if ((b2 * x * x + a2 * (y - 0.5f) * (y - 0.5f) - a2 * b2) >= 0)
				{
					y--;
				}
				if (x == 1 && y != yp)
				{
					FillRectangle(color, xc, yc + yp - 1, 1, 1);
					FillRectangle(color, xc, yc - yp, 1, 1);
				}
				if (y != yp)
				{
					FillRectangle(color, xc - x + 1, yc - yp, 2 * x - 1, 1);
					FillRectangle(color, xc - x + 1, yc + yp, 2 * x - 1, 1);
					yp = y;
					xp = x;
				}

				if (b2 * x >= a2 * y)
				{
					FillRectangle(color, xc - x, yc - yp, 2 * x + 1, 1);
					FillRectangle(color, xc - x, yc + yp, 2 * x + 1, 1);
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

					FillRectangle(color, xc - xp, yc - yp, 2 * xp + 1, divHeight);
					FillRectangle(color, xc - xp, yc + y + 1, 2 * xp + 1, divHeight);

					xp = x;
					yp = y;
				}

				if (y == 0)
				{
					divHeight = yp - y + 1;

					FillRectangle(color, xc - xp, yc - yp, 2 * x + 1, divHeight);
					FillRectangle(color, xc - xp, yc + y, 2 * x + 1, divHeight);
				}
			}
		}

		void Graphics::DrawString(const Misc::AnsiString &text, const FontPtr &font, const Color &color, const PointF &origin)
		{
			font->DrawText(buffer, text, origin, nullptr, color);

			buffer.SetActiveTexture(nullptr);
		}

		void Graphics::DrawString(const Misc::AnsiString &text, const FontPtr &font, const Color &color, float x, float y)
		{
			DrawString(text, font, color, PointF(x, y));
		}

		void Graphics::DrawImage(const ImagePtr &image, const ColorRectangle &color, const PointF &origin)
		{
			DrawImage(image, color, RectangleF(origin, image->GetSize()));
		}

		void Graphics::DrawImage(const std::shared_ptr<Image> &image, const ColorRectangle &color, const PointF &origin, const RectangleF &clip)
		{
			DrawImage(image, color, RectangleF(origin, image->GetSize()), clip);
		}

		void Graphics::DrawImage(const ImagePtr &image, const ColorRectangle &color, const RectangleF &area)
		{
			image->Render(buffer, area, nullptr, color);

			buffer.SetActiveTexture(nullptr);
		}

		void Graphics::DrawImage(const std::shared_ptr<Image> &image, const ColorRectangle &color, const RectangleF &area, const RectangleF &clip)
		{
			image->Render(buffer, area, &clip.OffsetEx(area.GetLocation()), color);
		}
	}
}
