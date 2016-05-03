/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_FONT_HPP
#define OSHGUI_DRAWING_FONT_HPP

#include "FontGlyph.hpp"
#include "Color.hpp"
#include "../Misc/Strings.hpp"
#include "Image.hpp"
#include <map>
#include <vector>
#include <memory>

#undef DrawText

namespace OSHGui {
	namespace Drawing {
		/**
		 * The class represents a font (type).
		 */
		class OSHGUI_EXPORT Font {
		public:
			/**
			 * Destructor
			 */
			virtual ~Font();

			/**
			 * Checks whether the corresponding code point can be drawn.
			 *
			 * \param cp Codepoint
			 * \return true, if the code point can be drawn
			 */
			bool IsCodepointAvailable(uint32_t cp) const {
				return (glyphMap.find(cp) != glyphMap.end());
			}

			/**
			 * Draws the text in the GeometryBuffer.
			 *
			 * \param buffer GeometryBuffer
			 * \param text Text
			 * \param position Position des Textes
			 * \param clip Clippingregion
			 * \param colors Farbe(n) der Glyphen
			 * \param spaceExtra zusätzlicher Platz bei einem Leerzeichen
			 * \return End-X Koordinate des gezeichneten Textes
			 */
			virtual float DrawText(GeometryBuffer &buffer, const Misc::UnicodeString &text, 
				const PointI &position, const RectangleF *clip, const ColorRectangle &colors, 
				const float spaceExtra = 0.0f) const;

			/**
			 * Gets the height of a line of text.
			 *
			 * \return Höhe einer Textzeile
			 */
			float GetLineSpacing() const {
				return height;
			}

			/**
			 * Gets the height of the font, in pixels.
			 *
			 * \return Height of the font
			 */
			float GetFontHeight() const {
				return ascender - descender;
			}

			/**
			 * Gets the height of the tallest glyph font, in pixels.
			 *
			 * \return Height of the tallest glyph font
			 */
			float GetBaseline() const {
				return ascender;
			}

			/**
			 * Gets the width of the drawn text, in pixels.
			 * This is the width actually used, while \ a GetTextAdvance 
			 * computes the theoretical width of the text.
			 *
			 * \return Breite des gezeichneten Textes
			 * \sa GetTextAdvance
			 */
			virtual float GetTextExtent(const Misc::UnicodeString &text) const;

			/**
			 * Gets the width of the theoretically drawn from the text, in pixels. 
			 * "Theoretically" means that oblique glyphs can be wider as their actual width.
			 *
			 * \return Breite des gezeichneten Textes
			 * \sa GetTextExtent
			 */
			virtual float GetTextAdvance(const Misc::UnicodeString &text) const;

			/**
			 * Gets the index of the character that is closest to the specified pixels.
			 *
			 * \param text Text
			 * \param pixel Pixel, dessen Index gesucht wird
			 * \return Index des gesuchten Zeichens [0, text.length()]
			 */
			size_t GetCharAtPixel(const Misc::UnicodeString &text, float pixel) const {
				return GetCharAtPixel(text, 0, pixel);
			}

			/**
			 * Ruft den Index des Zeichens ab, das am nächsten zu dem angegebenen Pixel liegt.
			 *
			 * \param text Text
			 * \param start Startindex im Text
			 * \param pixel Pixel, dessen Index gesucht wird
			 * \return Index des gesuchten Zeichens [0, text.length()]
			 */
			size_t GetCharAtPixel(const Misc::UnicodeString& text, size_t start, float pixel) const;

			/**
			 * Gets the to \ a codepoint belonging Glyph and possibly scans him.
			 *
			 * \param codepoint Codepoint
			 * \return nullptr, falls der Glyph nicht existiert
			 */
			const FontGlyph* GetGlyphData(uint32_t codepoint) const;

			/**
			* Specifies the screen size.
			*
			* @param size
			*/
			void DisplaySizeChanged(const SizeI &size);

		protected:
			/**
			 * Konstruktor der Klasse.
			 */
			Font();

			/**
			 * Initialisiert die Glyphen [\a startCodepoint, \a endCodepoint ]
			 *
			 * \param startCodepoint niedrigster Codepoint zum Rastern
			 * \param endCodepoint höchster Codepoint zum Rastern
			 */
			virtual void Rasterise(uint32_t startCodepoint, uint32_t endCodepoint) const;

			/**
			 * Aktuallisiert die Schrift, falls sich ihre Eigenschaften geändert haben.
			 */
			virtual void UpdateFont() = 0;

			/**
			 * Legt den maximalen Codepoint fest.
			 */
			void SetMaxCodepoint(uint32_t codepoint);

			/**
			 * Ruft den zugehörigen Glyphen ab.
			 *
			 * \param codepoint Codepoint
			 * \return nullptr, falls Glyph nicht vorhanden
			 */
			virtual const FontGlyph* FindFontGlyph(const uint32_t codepoint) const;

			float ascender;
			float descender;
			float height;

			uint32_t maximumCodepoint;

			mutable std::vector<uint32_t> loadedGlyphPages;

			typedef std::map<uint32_t, FontGlyph> CodepointMap;
			typedef CodepointMap::iterator CodepointIterator;
			mutable CodepointMap glyphMap;
		};

		typedef std::shared_ptr<Font> FontPtr;
	}
}

#endif
