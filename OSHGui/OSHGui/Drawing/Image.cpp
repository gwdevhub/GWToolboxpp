#include "Image.hpp"
#include "ImageLoader.hpp"
#include "CustomizableImage.hpp"
#include "../Application.hpp"
#include "../Misc/Exceptions.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Image::Image()
			: texture(nullptr),
			  area(0, 0, 0, 0),
			  offset(0, 0)
		{

		}
		//---------------------------------------------------------------------------
		Image::Image(TexturePtr _texture)
			: texture(std::move(_texture)),
			  area(PointF(0, 0), texture->GetSize()),
			  offset(0, 0)
		{

		}
		//---------------------------------------------------------------------------
		Image::Image(TexturePtr _texture, RectangleF _area, PointF _offset)
			: texture(std::move(_texture)),
			  area(std::move(_area)),
			  offset(std::move(_offset))
		{

		}
		//---------------------------------------------------------------------------
		Image::~Image()
		{

		}
		//---------------------------------------------------------------------------
		ImagePtr Image::FromFile(const Misc::AnsiString &file)
		{
			auto imageData = LoadImageFromFileToRGBABuffer(file);

			return FromBuffer(imageData.Data.data(), imageData.Size, Texture::PixelFormat::RGBA);
		}
		//---------------------------------------------------------------------------
		ImagePtr Image::FromMemory(Misc::RawDataContainer &data)
		{
			auto imageData = LoadImageFromContainerToRGBABuffer(data);

			return FromBuffer(imageData.Data.data(), imageData.Size, Texture::PixelFormat::RGBA);
		}
		//---------------------------------------------------------------------------
		ImagePtr Image::FromCustomizableImage(const CustomizableImage &image)
		{
			return FromBuffer(image.GetRGBAData().data(), image.GetSize(), Texture::PixelFormat::RGBA);
		}
		//---------------------------------------------------------------------------
		ImagePtr Image::FromBuffer(const void *data, const SizeF &size, Texture::PixelFormat format)
		{
			auto texture = Application::Instance().GetRenderer().CreateTexture();
			
			texture->LoadFromMemory(data, size, format);

			return std::make_shared<Image>(texture);
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		const SizeF& Image::GetSize() const
		{
			return area.GetSize();
		}
		//---------------------------------------------------------------------------
		const PointF& Image::GetOffset() const
		{
			return offset;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Image::Render(GeometryBuffer &buffer, const RectangleF &_area, const RectangleF *clip, const ColorRectangle &colors)
		{
			auto destination(_area);
			destination.Offset(offset);

			auto final_rect(clip ? destination.GetIntersection(*clip) : destination);
			if (final_rect.GetWidth() == 0 || final_rect.GetHeight() == 0)
			{
				return;
			}

			auto &scale = texture->GetTexelScaling();
			std::pair<float, float> pixelSize(area.GetWidth() / destination.GetWidth(), area.GetHeight() / destination.GetHeight());

			RectangleF textureRectangle((area.GetLocation() + ((final_rect.GetLocation() - destination.GetLocation()) * pixelSize)) * scale,
										(area.GetSize() + ((final_rect.GetSize() - destination.GetSize()) * pixelSize)) * scale);

			auto alignToPixels = [](float x)
			{
				return (float)(int)(( x ) + (( x ) > 0.0f ? 0.5f : -0.5f));
			};

			final_rect.SetLeft(alignToPixels(final_rect.GetLeft()));
			final_rect.SetTop(alignToPixels(final_rect.GetTop()));
			final_rect.SetHeight(alignToPixels(final_rect.GetHeight()));
			final_rect.SetWidth(alignToPixels(final_rect.GetWidth()));

			Vertex vertices[] = {
				{ Vector(final_rect.GetLeft(), final_rect.GetTop(), 0.0f),     colors.TopLeft,     PointF(textureRectangle.GetLeft(), textureRectangle.GetTop()) },
				{ Vector(final_rect.GetLeft(), final_rect.GetBottom(), 0.0f),  colors.BottomLeft,  PointF(textureRectangle.GetLeft(), textureRectangle.GetBottom()) },
				{ Vector(final_rect.GetRight(), final_rect.GetBottom(), 0.0f), colors.BottomRight, PointF(textureRectangle.GetRight(), textureRectangle.GetBottom()) },
				{ Vector(final_rect.GetRight(), final_rect.GetTop(), 0.0f),    colors.TopRight,    PointF(textureRectangle.GetRight(), textureRectangle.GetTop()) },
				{ Vector(final_rect.GetLeft(), final_rect.GetTop(), 0.0f),     colors.TopLeft,     PointF(textureRectangle.GetLeft(), textureRectangle.GetTop()) },
				{ Vector(final_rect.GetRight(), final_rect.GetBottom(), 0.0f), colors.BottomRight, PointF(textureRectangle.GetRight(), textureRectangle.GetBottom()) },
			};

			buffer.SetActiveTexture(texture);
			buffer.AppendGeometry(vertices, 6);
		}
		//---------------------------------------------------------------------------
		void Image::ComputeScalingFactors(const SizeF &displaySize, const SizeF &nativeDisplaySize, float &xScale, float &yScale)
		{
			xScale = displaySize.Height / nativeDisplaySize.Height;
			yScale = xScale;
		}
		//---------------------------------------------------------------------------
	}
}
