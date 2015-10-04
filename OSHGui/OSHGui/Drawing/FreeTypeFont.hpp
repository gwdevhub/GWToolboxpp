/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_FREETYPEFONT_HPP
#define OSHGUI_DRAWING_FREETYPEFONT_HPP

#include "Font.hpp"
#include "../Misc/RawDataContainer.hpp"
#include "Image.hpp"

#include <vector>

typedef struct FT_FaceRec_*  FT_Face;

#undef DrawText

namespace OSHGui
{
	namespace Drawing
	{
		/*!
		\brief
			Implementation of the Font class interface using the FreeType library.

			This implementation tries to provide maximal support for any kind of
			fonts supported by FreeType. It has been tested on outline font formats
			like TTF and PS as well as on bitmap font formats like PCF and FON.

			Glyphs are rendered dynamically on demand, so a large font with lots
			of glyphs won't slow application startup time.
		*/
		class OSHGUI_EXPORT FreeTypeFont : public Font
		{
		public:
			/*!
			\brief
				Constructor for FreeTypeFont based fonts.

			\param font_name
				The name that the font will use within the CEGUI system.

			\param point_size
				Specifies the point size that the font is to be rendered at.

			\param anti_aliased
				Specifies whether the font should be rendered using anti aliasing.

			\param font_filename
				The filename of an font file that will be used as the source for
				glyph images for this font.

			\param resource_group
				The resource group identifier to use when loading the font file
				specified by \a font_filename.

			\param auto_scaled
				Specifies whether the font imagery should be automatically scaled to
				maintain the same physical size (which is calculated by using the
				native resolution setting).

			\param native_horz_res
				The horizontal native resolution value.  This is only significant when
				auto scaling is enabled.

			\param native_vert_res
				The vertical native resolution value.  This is only significant when
				auto scaling is enabled.

			\param specific_line_spacing
				If specified (non-zero), this will be the line spacing that we will
				report for this font, regardless of what is mentioned in the font file
				itself.
			*/
			FreeTypeFont(const Misc::AnsiString &filename, const float pointSize, const bool antiAliased, const float lineSpacing = 0.0f);

			FreeTypeFont(Misc::RawDataContainer data, const float pointSize, const bool antiAliased, const float lineSpacing = 0.0f);

			//! Destructor.
			~FreeTypeFont();

			//! return the point size of the freetype font.
			float GetPointSize() const;

			//! return whether the freetype font is rendered anti-aliased.
			bool IsAntiAliased() const;

			//! return the point size of the freetype font.
			void SetPointSize(const float pointSize);

			//! return whether the freetype font is rendered anti-aliased.
			void SetAntiAliased(const bool antiAliased);

		protected:
			/*!
			\brief
				Copy the current glyph data into \a buffer, which has a width of
				\a buf_width pixels (not bytes).

			\param buffer
				Memory buffer large enough to receive the imagery for the currently
				loaded glyph.

			\param buf_width
				Width of \a buffer in pixels (where each pixel is a argb_t).

			\return
				Nothing.
			*/
			void DrawGlyphToBuffer(uint32_t *buffer, uint32_t width) const;

			/*!
			\brief
				Return the required texture size required to store imagery for the
				glyphs from s to e
			\param s
				The first glyph in set
			\param e
				The last glyph in set
			*/
			unsigned int GetTextureSize(CodepointIterator start, CodepointIterator end) const;

			//! Free all allocated font data.
			void Free();

			//! initialise FontGlyph for given codepoint.
			void InitialiseFontGlyph(CodepointIterator cp) const;

			void InitialiseGlyphMap();

			// overrides of functions in Font base class.
			virtual const FontGlyph* FindFontGlyph(const uint32_t codepoint) const override;
			virtual void Rasterise(uint32_t start_codepoint, uint32_t end_codepoint) const override;
			virtual void UpdateFont() override;

			//! If non-zero, the overridden line spacing that we're to report.
			float lineSpacing;
			//! Point size of font.
			float pointSize;
			//! True if the font should be rendered as anti-alaised by freeType.
			bool antiAliased;
			//! FreeType-specific font handle
			FT_Face fontFace;
			//! Font file data
			Misc::RawDataContainer data;
			//! Type definition for TextureVector.
			typedef std::vector<TexturePtr> TextureVector;
			//! Textures that hold the glyph imagery for this font.
			mutable TextureVector glyphTextures;
			typedef std::vector<ImagePtr> ImageVector;
			//! collection of images defined for this font.
			mutable ImageVector glyphImages;
		};
	}
}

#endif
