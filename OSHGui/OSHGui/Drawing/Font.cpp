#include "Font.hpp"
#include "../Application.hpp"
#include <algorithm>

#undef DrawText

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//static attributes
		//---------------------------------------------------------------------------
		const auto BitsPerUnit = sizeof(uint32_t) * 8;
		const auto GlyphsPerPage = 256;
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Font::Font()
			: ascender(0.0f),
			  descender(0.0f),
			  height(0.0f),
			  maximumCodepoint(0),
			  scalingHorizontal(1.0f),
			  scalingVertical(1.0f)
		{
			
		}
		//---------------------------------------------------------------------------
		Font::~Font()
		{
			
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		void Font::SetMaxCodepoint(uint32_t codepoint)
		{
			loadedGlyphPages.clear();

			maximumCodepoint = codepoint;

			auto pages = (codepoint + GlyphsPerPage) / GlyphsPerPage;
			auto size = (pages + BitsPerUnit - 1) / BitsPerUnit;

			loadedGlyphPages.resize(size * sizeof(uint32_t));
		}
		//---------------------------------------------------------------------------
		const FontGlyph* Font::GetGlyphData(uint32_t codepoint) const
		{
			if (codepoint > maximumCodepoint)
			{
				return nullptr;
			}

			auto glyph = FindFontGlyph(codepoint);

			if (!loadedGlyphPages.empty())
			{
				auto page = codepoint / GlyphsPerPage;
				auto mask = 1 << (page & (BitsPerUnit - 1));
				if (!(loadedGlyphPages[page / BitsPerUnit] & mask))
				{
					loadedGlyphPages[page / BitsPerUnit] |= mask;
					Rasterise(codepoint & ~(GlyphsPerPage - 1), codepoint | (GlyphsPerPage - 1));
				}
			}

			return glyph;
		}
		//---------------------------------------------------------------------------
		const FontGlyph* Font::FindFontGlyph(const uint32_t codepoint) const
		{
			auto pos = glyphMap.find(codepoint);
			return pos != glyphMap.end() ? &pos->second : nullptr;
		}
		//---------------------------------------------------------------------------
		float Font::GetTextExtent(const Misc::UnicodeString &text, float scaleX) const
		{
			float current = 0.f;
			float advance = 0.f;

			for (auto c : text)
			{
				auto glyph = GetGlyphData((unsigned char)c);
				if (glyph)
				{
					auto width = glyph->GetRenderedAdvance(scaleX);

					if (advance + width > current)
					{
						current = advance + width;
					}

					advance += glyph->GetAdvance(scaleX);
				}
			}

			return std::max(advance, current);
		}
		//---------------------------------------------------------------------------
		float Font::GetTextAdvance(const Misc::UnicodeString &text, float scaleX) const
		{
			float advance = 0.0f;

			for (auto c : text)
			{
				if (auto glyph = GetGlyphData((unsigned char)c))
				{
					advance += glyph->GetAdvance(scaleX);
				}
			}

			return advance;
		}
		//---------------------------------------------------------------------------
		size_t Font::GetCharAtPixel(const Misc::UnicodeString &text, size_t start, float pixel, float scaleX) const
		{
			float current = 0.f;
			auto length = text.length();

			if (pixel <= 0.f || length <= start)
			{
				return start;
			}

			for (auto c = start; c < length; ++c)
			{
				auto glyph = GetGlyphData((unsigned char)text[c]);
				if (glyph)
				{
					current += glyph->GetAdvance(scaleX);

					if (pixel < current)
					{
						return c;
					}
				}
			}

			return length;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Font::DisplaySizeChanged(const SizeF &size)
		{
			//Image::ComputeScalingFactors(size, d_nativeResolution, scalingHorizontal, scalingVertical);

			UpdateFont();
		}
		//---------------------------------------------------------------------------
		float Font::DrawText(GeometryBuffer &buffer, const Misc::UnicodeString &text, const PointF &position, const RectangleF *clip, const ColorRectangle &colors, const float spaceExtra, const float scaleX, const float scaleY) const
		{
			auto base = position.Y + GetBaseline(scaleY);
			auto glyphPosition(position);

			for (auto c : text)
			{
				if (auto glyph = GetGlyphData((unsigned char)c))
				{
					auto image = glyph->GetImage();
					glyphPosition.Y = base - (image->GetOffset().Y - image->GetOffset().Y * scaleY);
					image->Render(buffer, RectangleF(glyphPosition, glyph->GetSize(scaleX, scaleY)), clip, colors);
					glyphPosition.X += glyph->GetAdvance(scaleX);// - 1.f;

					if (c == ' ')
					{
						glyphPosition.X += spaceExtra;
					}
				}
			}

			return glyphPosition.X;
		}
		//---------------------------------------------------------------------------
		void Font::Rasterise(uint32_t startCodepoint, uint32_t endCodepoint) const
		{
			
		}
		//---------------------------------------------------------------------------
	}
}
