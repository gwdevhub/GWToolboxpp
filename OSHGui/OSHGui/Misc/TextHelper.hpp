/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_MISC_TEXTHELPER_HPP
#define OSHGUI_MISC_TEXTHELPER_HPP

#include <memory>
#include "../Exports.hpp"
#include "Strings.hpp"
#include "../Drawing/Font.hpp"
#include "../Drawing/Point.hpp"
#include "../Drawing/Size.hpp"

namespace OSHGui
{
	namespace Misc
	{
		/**
		 * Verwaltet eine Zeichenkette und deren Ausmaße.
		 */
		class OSHGUI_EXPORT TextHelper
		{
		public:
			/**
			 * Erstellt ein TextHelper-Objekt mit der entsprechenden Schriftart.
			 *
			 * \param font
			 */
			TextHelper(const Drawing::FontPtr &font);
			
			/**
			 * Legt die Schriftart fest.
			 *
			 * \param font darf nicht 0 sein
			 */
			void SetFont(const Drawing::FontPtr &font);
			/**
			 * Legt den Text fest.
			 *
			 * \param text
			 */
			void SetText(const AnsiString &text);
			/**
			 * Hängt das Zeichen an den Text an.
			 *
			 * \param character
			 */
			void Append(const AnsiChar character);
			/**
			 * Hängt die Zeichenkette an den Text an.
			 *
			 * \param text
			 */
			void Append(const AnsiString &text);
			/**
			 * Fügt das Zeichen an der entsprechenden Position im Text ein.
			 *
			 * \param position
			 * \param character
			 */
			void Insert(int position, const AnsiChar character);
			/**
			 * Fügt die Zeichenkette an der entsprechenden Position im Text ein.
			 *
			 * \param position
			 * \param text
			 */
			void Insert(int position, const AnsiString &text);
			/**
			 * Löscht den kompletten Text.
			 */
			void Clear();
			/**
			 * Löscht das Zeichen an der entsprechenden Stelle.
			 *
			 * \param index die Stelle ab der gelöscht werden soll
			 * \param length Anzahl der zu löschenden Zeichen (default: 1)
			 */
			void Remove(int index, int length = 1);
			
			/**
			 * Ruft die Länge der Zeichenkette ab.
			 *
			 * \return length
			 */
			int GetLength() const;
			/**
			 * Ruft die Zeichenkette ab.
			 *
			 * \return text
			 */
			const AnsiString& GetText() const;
			/**
			 * Ruft die Maße der Zeichenkette in Pixel ab.
			 *
			 * \return size
			 */
			const Drawing::SizeF& GetSize() const;
			
			/**
			 * Berechnet die Maße neu.
			 */
			void RefreshSize();
			
			/**
			 * Gibt die Position des Zeichens an der entsprechenden Stelle zurück.
			 *
			 * \param index
			 * \param trailing linker Rand des Zeichens / rechter Rand des Zeichens
			 * \return position
			 */
			Drawing::PointF GetCharacterPosition(int index, bool trailing = false) const;
			/**
			 * Gibt die Größe eines Teilstrings zurück.
			 *
			 * \param index der Startindex
			 * \param size die Länge der Zeichenkette. -1 = bis zum Ende (default: -1)
			 * \return die Größe
			 */
			Drawing::SizeF GetStringSize(int index, int size = -1) const;
			/**
			 * Gibt den Index des Zeichens zurück, das der Position am nächsten steht.
			 *
			 * \param position
			 * \return index
			 */
			int GetClosestCharacterIndex(const Drawing::PointF &position) const;
			
		private:
			AnsiString text_;
			
			Drawing::SizeF size_;
			
			Drawing::FontPtr font_;
		};
	}
}

#endif