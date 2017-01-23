#include "Image.hpp"
#include "ImageLoader.hpp"
#include "CustomizableImage.hpp"
#include "../Application.hpp"
#include "../Misc/Exceptions.hpp"

namespace OSHGui {
	namespace Drawing {
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Image::Image()
			: texture(nullptr),
			  area(0, 0, 0, 0),
			  offset(0, 0) {

		}
		//---------------------------------------------------------------------------
		Image::Image(TexturePtr _texture)
			: texture(std::move(_texture)),
			  area(PointI(0, 0), texture->GetSize()),
			  offset(0, 0) {

		}
		//---------------------------------------------------------------------------
		Image::Image(TexturePtr _texture, RectangleI _area, PointI _offset)
			: texture(std::move(_texture)),
			  area(std::move(_area)),
			  offset(std::move(_offset)) {

		}
		//---------------------------------------------------------------------------
		Image::~Image() {

		}
		//---------------------------------------------------------------------------
		ImagePtr Image::FromFile(const Misc::AnsiString &file) {
			auto imageData = LoadImageFromFileToRGBABuffer(file);
			return FromBuffer(imageData.Data.data(), imageData.Size, Texture::PixelFormat::RGBA);
		}
		//---------------------------------------------------------------------------
		ImagePtr Image::FromMemory(Misc::RawDataContainer &data) {
			auto imageData = LoadImageFromContainerToRGBABuffer(data);
			return FromBuffer(imageData.Data.data(), imageData.Size, Texture::PixelFormat::RGBA);
		}
		//---------------------------------------------------------------------------
		ImagePtr Image::FromCustomizableImage(const CustomizableImage &image){
			return FromBuffer(image.GetRGBAData().data(), image.GetSize(), Texture::PixelFormat::RGBA);
		}
		//---------------------------------------------------------------------------
		ImagePtr Image::FromBuffer(const void *data, const SizeI &size, Texture::PixelFormat format) {
			auto texture = Application::Instance().GetRenderer().CreateTexture();
			
			texture->LoadFromMemory(data, size, format);

			return std::make_shared<Image>(texture);
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		const SizeI& Image::GetSize() const {
			return area.GetSize();
		}
		//---------------------------------------------------------------------------
		const PointI& Image::GetOffset() const {
			return offset;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Image::Render(GeometryBuffer &buffer, const RectangleF &_area, const RectangleF *clip, const ColorRectangle &colors) {
			RectangleF areaf(area.cast<float>());
			RectangleF destination(_area);
			destination.Offset(offset.cast<float>());
			RectangleF final_rect(clip ? destination.GetIntersection(*clip) : destination);
			if (final_rect.GetWidth() == 0 || final_rect.GetHeight() == 0) {
				return;
			}

			auto &scale = texture->GetTexelScaling();
			std::pair<float, float> pixelSize(areaf.GetWidth() / destination.GetWidth(), areaf.GetHeight() / destination.GetHeight());
			
			RectangleF textureRectangle((areaf.GetLocation() + ((final_rect.GetLocation() - destination.GetLocation()) * pixelSize)) * scale,
										(areaf.GetSize()	 + ((final_rect.GetSize()	  - destination.GetSize())	   * pixelSize)) * scale);

			Vertex vertices[] = {
				{ final_rect.GetTopLeft(),		colors.TopLeft,		textureRectangle.GetTopLeft() },
				{ final_rect.GetBottomLeft(),	colors.BottomLeft,  textureRectangle.GetBottomLeft() },
				{ final_rect.GetBottomRight(),	colors.BottomRight, textureRectangle.GetBottomRight() },
				{ final_rect.GetTopRight(),		colors.TopRight,    textureRectangle.GetTopRight() },
				{ final_rect.GetTopLeft(),		colors.TopLeft,     textureRectangle.GetTopLeft() },
				{ final_rect.GetBottomRight(),	colors.BottomRight, textureRectangle.GetBottomRight() }
			};

			buffer.SetActiveTexture(texture);
			buffer.AppendGeometry(vertices, 6);
		}
		//---------------------------------------------------------------------------
		void Image::ComputeScalingFactors(const SizeF &displaySize, const SizeF &nativeDisplaySize, float &xScale, float &yScale) {
			xScale = displaySize.Height / nativeDisplaySize.Height;
			yScale = xScale;
		}
		//---------------------------------------------------------------------------
	}
}
