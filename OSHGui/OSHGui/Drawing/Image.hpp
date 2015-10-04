/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_IMAGE_HPP
#define OSHGUI_DRAWING_IMAGE_HPP

#include "../Misc/Strings.hpp"
#include "../Misc/RawDataContainer.hpp"
#include "Texture.hpp"
#include "GeometryBuffer.hpp"
#include "ColorRectangle.hpp"
#include "Rectangle.hpp"
#include <memory>

namespace OSHGui
{
	namespace Drawing
	{
		class Graphics;
		class Font;
		class CustomizableImage;

		class OSHGUI_EXPORT Image
		{
		public:
			/**
			 * Constructs an empty image.
			 */
			Image();
			/**
			 * Constructs an image with the specific texture.
			 *
			 * \param texture
			 */
			Image(TexturePtr texture);
			/**
			 * Constructs an image with the specific texture and specify the area of the texture which defines the image.
			 *
			 * \param texture
			 * \param area
			 * \param offset
			 */
			Image(TexturePtr texture, RectangleF area, PointF offset);
			/**
			 * Destructor
			 */
			virtual ~Image();

			/**
			 * Loads an image from a file. (BMP, JPEG, GIF, PNG, TGA, ICO)
			 *
			 * \param filename Path to the file
			 * \return the loaded image
			 */
			static std::shared_ptr<Image> FromFile(const Misc::AnsiString &filename);
			/**
			 * Loads an image from a memory container. (BMP, JPEG, GIF, PNG, TGA, ICO)
			 *
			 * \param data the loaded image in memory
			 * \return the loaded image
			 */
			static std::shared_ptr<Image> FromMemory(Misc::RawDataContainer &data);
			/**
			* Loads an image from a CustomizableImage.
			*
			* \param image the CustomizableImage
			* \return the loaded image
			*/
			static std::shared_ptr<Image> FromCustomizableImage(const CustomizableImage &image);
			/**
			 * Loads an image from a buffer which holds the data in the specifed format.
			 *
			 * \param data
			 * \param size the size of the image in pixels
			 * \param format the PixelFormat
			 * \return the loaded image
			 */
			static std::shared_ptr<Image> FromBuffer(const void *data, const SizeF &size, Texture::PixelFormat format);

			static void ComputeScalingFactors(const SizeF &displaySize, const SizeF &nativeDisplaySize, float &xScale, float &yScale);

			virtual const SizeF& GetSize() const;
			virtual const PointF& GetOffset() const;

		protected:
			friend class Graphics;
			friend class Font;

			virtual void Render(GeometryBuffer &buffer, const RectangleF &area, const RectangleF *clip, const ColorRectangle &color);

		private:
			TexturePtr texture;
			RectangleF area;
			PointF offset;
		};

		typedef std::shared_ptr<Image> ImagePtr;
	}
}

#endif
