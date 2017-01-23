#include "Font.hpp"
#include "../Application.hpp"
#include <algorithm>

#undef DrawText

namespace OSHGui {
	namespace Drawing {
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
			  maximumCodepoint(0) {
		}
		//---------------------------------------------------------------------------
		Font::~Font(){	
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		void Font::SetMaxCodepoint(uint32_t codepoint) {
			loadedGlyphPages.clear();

			maximumCodepoint = codepoint;

			unsigned int pages = (codepoint + GlyphsPerPage) / GlyphsPerPage;
			unsigned int size = (pages + BitsPerUnit - 1) / BitsPerUnit;

			loadedGlyphPages.resize(size * sizeof(uint32_t));
		}
		//---------------------------------------------------------------------------
		const FontGlyph* Font::GetGlyphData(uint32_t codepoint) const {
			if (codepoint > maximumCodepoint) {
				return nullptr;
			}

			auto glyph = FindFontGlyph(codepoint);

			if (!loadedGlyphPages.empty()) {
				auto page = codepoint / GlyphsPerPage;
				auto mask = 1 << (page & (BitsPerUnit - 1));
				if (!(loadedGlyphPages[page / BitsPerUnit] & mask)) {
					loadedGlyphPages[page / BitsPerUnit] |= mask;
					Rasterise(codepoint & ~(GlyphsPerPage - 1), codepoint | (GlyphsPerPage - 1));
				}
			}

			return glyph;
		}
		//---------------------------------------------------------------------------
		const FontGlyph* Font::FindFontGlyph(const uint32_t codepoint) const {
			auto pos = glyphMap.find(codepoint);
			return pos != glyphMap.end() ? &pos->second : nullptr;
		}
		//---------------------------------------------------------------------------
		float Font::GetTextExtent(const Misc::UnicodeString &text) const {
			float current = 0.f;
			float advance = 0.f;

			for (wchar_t c : text) {
				auto glyph = GetGlyphData((unsigned char)c);
				if (glyph) {
					int width = glyph->GetRenderedAdvance();

					if (advance + width > current) {
						current = advance + width;
					}

					advance += glyph->GetAdvance();
				}
			}

			return std::max(advance, current);
		}
		//---------------------------------------------------------------------------
		float Font::GetTextAdvance(const Misc::UnicodeString &text) const {
			float advance = 0.0f;

			for (auto c : text) {
				if (auto glyph = GetGlyphData((unsigned char)c)) {
					advance += glyph->GetAdvance();
				}
			}

			return advance;
		}
		//---------------------------------------------------------------------------
		size_t Font::GetCharAtPixel(const Misc::UnicodeString &text, size_t start, float pixel) const {
			float current = 0.f;
			auto length = text.length();

			if (pixel <= 0.f || length <= start) {
				return start;
			}

			for (auto c = start; c < length; ++c) {
				auto glyph = GetGlyphData((unsigned char)text[c]);
				if (glyph) {
					current += glyph->GetAdvance();

					if (pixel < current) {
						return c;
					}
				}
			}

			return length;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Font::DisplaySizeChanged(const SizeI &size) {
			UpdateFont();
		}
		//---------------------------------------------------------------------------
		float Font::DrawText(GeometryBuffer &buffer, const Misc::UnicodeString &text, const PointI &position, 
			const RectangleF *clip, const ColorRectangle &colors, const float spaceExtra) const {
			PointF glyphPosition = position.cast<float>(); // has to be float because of x
			glyphPosition.Y += std::round(GetBaseline());

			for (wchar_t c : text) {
				if (const Drawing::FontGlyph* glyph = GetGlyphData((unsigned char)c)) {
					Drawing::ImagePtr image = glyph->GetImage();
					image->Render(buffer, RectangleF(glyphPosition, glyph->GetSize().cast<float>()), clip, colors);
					glyphPosition.X += glyph->GetAdvance();

					if (c == ' ') {
						glyphPosition.X += spaceExtra;
					}
				}
			}

			return glyphPosition.X;
		}
		//---------------------------------------------------------------------------
		void Font::Rasterise(uint32_t startCodepoint, uint32_t endCodepoint) const {
		}
		//---------------------------------------------------------------------------
	}
}
