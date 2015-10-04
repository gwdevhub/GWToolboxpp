#include "FreeTypeFont.hpp"
#include "Texture.hpp"
#include "../Application.hpp"
#include "../Misc/Exceptions.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#undef DrawText

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//static attributes
		//---------------------------------------------------------------------------
		const unsigned int GlyphPadding = 2;
		const float FT_PosCoefficient = 1.0f / 64.0f;
		//---------------------------------------------------------------------------
		FT_Library freeType = nullptr;
		int freeTypeUsageCounter = 0;
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		FreeTypeFont::FreeTypeFont(const Misc::AnsiString &filename, const float _pointSize, const bool _antiAliased, const float _lineSpacing)
			: lineSpacing(_lineSpacing),
			  pointSize(_pointSize),
			  antiAliased(_antiAliased),
			  fontFace(nullptr)
		{
			if (!freeTypeUsageCounter++)
			{
				FT_Init_FreeType(&freeType);
			}

			data.LoadFromFile(filename);

			UpdateFont();
		}
		//---------------------------------------------------------------------------
		FreeTypeFont::FreeTypeFont(Misc::RawDataContainer _data, const float _pointSize, const bool _antiAliased, const float _lineSpacing)
			: lineSpacing(_lineSpacing),
			  pointSize(_pointSize),
			  antiAliased(_antiAliased),
			  fontFace(nullptr),
			  data(std::move(_data))
		{
			if (!freeTypeUsageCounter++)
			{
				FT_Init_FreeType(&freeType);
			}

			UpdateFont();
		}
		//---------------------------------------------------------------------------
		FreeTypeFont::~FreeTypeFont()
		{
			Free();

			if (!--freeTypeUsageCounter)
			{
				FT_Done_FreeType(freeType);
			}
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		float FreeTypeFont::GetPointSize() const
		{
			return pointSize;
		}
		//---------------------------------------------------------------------------
		void FreeTypeFont::SetPointSize(const float _pointSize)
		{
			if (pointSize == _pointSize)
			{
				return;
			}

			pointSize = _pointSize;

			UpdateFont();
		}
		//---------------------------------------------------------------------------
		bool FreeTypeFont::IsAntiAliased() const
		{
			return antiAliased;
		}
		//---------------------------------------------------------------------------
		void FreeTypeFont::SetAntiAliased(const bool _antiAliasing)
		{
			if (antiAliased == _antiAliasing)
			{
				return;
			}

			antiAliased = _antiAliasing;

			UpdateFont();
		}
		//---------------------------------------------------------------------------
		const FontGlyph* FreeTypeFont::FindFontGlyph(const uint32_t codepoint) const
		{
			auto it = glyphMap.find(codepoint);
			if (it == glyphMap.end())
			{
				return nullptr;
			}

			if (!it->second.IsValid())
			{
				InitialiseFontGlyph(it);
			}

			return &it->second;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		uint32_t FreeTypeFont::GetTextureSize(CodepointIterator start, CodepointIterator end) const
		{
			auto size = 32;
			auto maximum = Application::Instance().GetRenderer().GetMaximumTextureSize();
			auto count = 0;

			while (size < maximum)
			{
				auto x = GlyphPadding;
				auto y = GlyphPadding;
				auto yb = GlyphPadding;
				for (auto c = start; c != end; ++c)
				{
					if (c->second.GetImage())
					{
						continue;
					}

					if (FT_Load_Char(fontFace, c->first, FT_LOAD_DEFAULT | FT_LOAD_FORCE_AUTOHINT) != 0)
					{
						continue;
					}

					auto glyphWidth = (int)std::ceil(fontFace->glyph->metrics.width * FT_PosCoefficient) + GlyphPadding;
					auto glyphHeight = (int)std::ceil(fontFace->glyph->metrics.height * FT_PosCoefficient) + GlyphPadding;

					x += glyphWidth;
					if (x > size)
					{
						x = GlyphPadding;
						y = yb;
					}
					auto yy = y + glyphHeight;
					if (yy > size)
					{
						goto too_small;
					}

					if (yy > yb)
					{
						yb = yy;
					}

					++count;
				}
				break;

				too_small:
				size *= 2;
			}

			return count ? size : 0;
		}
		//---------------------------------------------------------------------------
		void FreeTypeFont::Rasterise(uint32_t startCodepoint, uint32_t endCodepoint) const
		{
			auto start = glyphMap.lower_bound(startCodepoint);
			if (start == glyphMap.end())
			{
				return;
			}

			auto bck = start;
			auto end = glyphMap.upper_bound(endCodepoint);
			while (true)
			{
				auto textureSize = GetTextureSize(start, end);
				if (textureSize == 0)
				{
					break;
				}

				auto texture = Application::Instance().GetRenderer().CreateTexture(SizeF(textureSize, textureSize));
				glyphTextures.push_back(texture);

				std::vector<uint32_t> buffer(textureSize * textureSize);
				
				auto x = GlyphPadding;
				auto y = GlyphPadding;
				auto yb = GlyphPadding;

				bool finished = false;
				bool forward = true;

				while (start != glyphMap.end())
				{
					finished |= (start == end);

					if (!start->second.GetImage())
					{
						if (FT_Load_Char(fontFace, start->first, FT_LOAD_RENDER | (antiAliased ? FT_LOAD_TARGET_NORMAL : FT_LOAD_TARGET_MONO)) != 0)
						{
							auto image = std::make_shared<Image>(texture, RectangleF(0, 0, 0, 0), PointF(0, 0));
							glyphImages.push_back(image);
							start->second.SetImage(image);
						}
						else
						{
							auto glyphWidth = fontFace->glyph->bitmap.width + GlyphPadding;
							auto glyphHeight = fontFace->glyph->bitmap.rows + GlyphPadding;

							auto next = x + glyphWidth;
							if (next > textureSize)
							{
								x = GlyphPadding;
								next = x + glyphWidth;
								y = yb;
							}

							auto bottom = y + glyphHeight;
							if (bottom > textureSize)
							{
								break;
							}

							DrawGlyphToBuffer(buffer.data() + (y * textureSize) + x, textureSize);

							RectangleF area(x, y, glyphWidth - GlyphPadding, glyphHeight - GlyphPadding);
							PointF offset(fontFace->glyph->metrics.horiBearingX * FT_PosCoefficient, -fontFace->glyph->metrics.horiBearingY * FT_PosCoefficient);

							auto image = std::make_shared<Image>(texture, area, offset);
							glyphImages.push_back(image);
							start->second.SetImage(image);

							x = next;
							if (bottom > yb)
							{
								yb = bottom;
							}
						}
					}

					if (forward)
					{
						if (++start == glyphMap.end())
						{
							finished = true;
							forward = false;
							start = bck;
						}
					}
					if (!forward)
					{
						if ((start == glyphMap.begin()) || (--start == glyphMap.begin()))
						{
							break;
						}
					}
				}

				texture->LoadFromMemory(buffer.data(), SizeF(textureSize, textureSize), Texture::PixelFormat::RGBA);

				if (finished)
				{
					break;
				}
			}
		}
		//---------------------------------------------------------------------------
		void FreeTypeFont::DrawGlyphToBuffer(uint32_t *buffer, uint32_t width) const
		{
			auto bitmap = &fontFace->glyph->bitmap;

			for (auto i = 0; i < bitmap->rows; ++i)
			{
				auto src = bitmap->buffer + (i * bitmap->pitch);
				switch (bitmap->pixel_mode)
				{
					case FT_PIXEL_MODE_GRAY:
						{
							auto dst = reinterpret_cast<uint8_t*>(buffer);
							for (int j = 0; j < bitmap->width; ++j)
							{
								*dst++ = 0xFF;
								*dst++ = 0xFF;
								*dst++ = 0xFF;
								*dst++ = *src++;
							}
						}
						break;
					case FT_PIXEL_MODE_MONO:
						for (auto j = 0; j < bitmap->width; ++j)
						{
							buffer[j] = (src[j / 8] & (0x80 >> (j & 7))) ? 0xFFFFFFFF : 0x00000000;
						}
						break;
					default:
						throw;
				}

				buffer += width;
			}
		}
		//---------------------------------------------------------------------------
		void FreeTypeFont::Free()
		{
			if (!fontFace)
			{
				return;
			}

			glyphMap.clear();
			glyphImages.clear();
			glyphTextures.clear();

			FT_Done_Face(fontFace);
			fontFace = nullptr;
		}
		//---------------------------------------------------------------------------
		void FreeTypeFont::UpdateFont()
		{
			Free();

			if (FT_New_Memory_Face(freeType, data.GetDataPointer(), static_cast<FT_Long>(data.GetSize()), 0, &fontFace) != 0)
			{
				throw Misc::Exception();
			}

			if (!fontFace->charmap)
			{
				FT_Done_Face(fontFace);
				fontFace = nullptr;

				throw Misc::Exception();
			}

			auto dpiHorizontal = static_cast<uint32_t>(Application::Instance().GetRenderer().GetDisplayDPI().X);
			auto dpiVertical = static_cast<uint32_t>(Application::Instance().GetRenderer().GetDisplayDPI().Y);

			auto hps = pointSize * 64.0f;
			auto vps = pointSize * 64.0f;

			if (FT_Set_Char_Size(fontFace, FT_F26Dot6(hps), FT_F26Dot6(vps), dpiHorizontal, dpiVertical) != 0)
			{
				auto ptSize72 = (pointSize * 72.0f) / dpiVertical;
				auto bestDelta = 99999.0f;
				auto bestSize = 0.0f;
				for (int i = 0; i < fontFace->num_fixed_sizes; i++)
				{
					float size = fontFace->available_sizes[i].size * FT_PosCoefficient;
					float delta = std::abs(size - ptSize72);
					if (delta < bestDelta)
					{
						bestDelta = delta;
						bestSize = size;
					}
				}

				if (bestSize <= 0.0f || FT_Set_Char_Size(fontFace, 0, FT_F26Dot6(bestSize * 64), 0, 0) != 0)
				{
					throw Misc::Exception();
				}
			}

			if (fontFace->face_flags & FT_FACE_FLAG_SCALABLE)
			{
				float scaleY = fontFace->size->metrics.y_scale * FT_PosCoefficient * (1.0f / 65536.0f);
				ascender = fontFace->ascender * scaleY;
				descender = fontFace->descender * scaleY;
				height = fontFace->height * scaleY;
			}
			else
			{
				ascender = fontFace->size->metrics.ascender * FT_PosCoefficient;
				descender = fontFace->size->metrics.descender * FT_PosCoefficient;
				height = fontFace->size->metrics.height * FT_PosCoefficient;
			}

			if (lineSpacing > 0.0f)
			{
				height = lineSpacing;
			}

			InitialiseGlyphMap();
		}
		//---------------------------------------------------------------------------
		void FreeTypeFont::InitialiseGlyphMap()
		{
			FT_UInt index;
			auto codepoint = FT_Get_First_Char(fontFace, &index);
			auto maximum = codepoint;

			while (index)
			{
				if (maximum < codepoint)
				{
					maximum = codepoint;
				}

				glyphMap[codepoint] = FontGlyph();

				codepoint = FT_Get_Next_Char(fontFace, codepoint, &index);
			}

			SetMaxCodepoint(maximum);
		}
		//---------------------------------------------------------------------------
		void FreeTypeFont::InitialiseFontGlyph(CodepointIterator it) const
		{
			if (FT_Load_Char(fontFace, it->first, FT_LOAD_DEFAULT) != 0)
			{
				return;
			}

			auto advance = fontFace->glyph->metrics.horiAdvance * FT_PosCoefficient;

			it->second.SetAdvance(advance);
			it->second.SetValid(true);
		}
		//---------------------------------------------------------------------------
	}
}
