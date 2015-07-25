/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_MISC_STRINGS_HPP
#define OSHGUI_MISC_STRINGS_HPP

#include <string>
#include <stdarg.h>
#include "../Exports.hpp"

namespace OSHGui
{
	namespace Misc
	{
		/**
		 * Stellt Text als eine Reihe von Unicode-Zeichen dar.
		 */
		typedef std::wstring UnicodeString;
		/**
		 * Stellt ein Unicode-Zeichen dar.
		 */
		typedef wchar_t UnicodeChar;
		/**
		 * Stellt Text als eine Reihe von Ansi-Zeichen dar.
		 */
		typedef std::string AnsiString;
		/**
		 * Stellt ein Ansi-Zeichen dar.
		 */
		typedef char AnsiChar;
		
		namespace String
		{
			enum Latin1Category
			{
				Control,
				Letter,
				DecimalDigitNumber,
				SpaceSeparator,
				OtherNumber,
				OtherSymbol,
				LowercaseLetter,
				UppercaseLetter,
				ModifierSymbol,
				CurrencySymbol,
				MathSymbol,
				OpenPunctuation,
				ClosePunctuation,
				OtherPunctuation,
				DashPunctuation,
				ConnectorPunctuation,
				InitialQuotePunctuation,
				FinalQuotePunctuation
			};

			/**
			 * Ruft die Latin1Category des Zeichens ab.
			 *
			 * \param c das Zeichen
			 * \return die Latin1Category
			 */
			inline Latin1Category GetLatin1Category(const AnsiChar c)
			{
				static Latin1Category latin1Category[] =
				{
					Control, Control, Control, Control, Control, Control, Control, Control, // 0000 - 0007
					Control, Control, Control, Control, Control, Control, Control, Control, // 0008 - 000F
					Control, Control, Control, Control, Control, Control, Control, Control, // 0010 - 0017
					Control, Control, Control, Control, Control, Control, Control, Control, // 0018 - 001F
					SpaceSeparator, OtherPunctuation, OtherPunctuation, OtherPunctuation, CurrencySymbol, OtherPunctuation, OtherPunctuation, OtherPunctuation, // 0020 - 0027
					OpenPunctuation, ClosePunctuation, OtherPunctuation, MathSymbol, OtherPunctuation, DashPunctuation, OtherPunctuation, OtherPunctuation, // 0028 - 002F
					DecimalDigitNumber, DecimalDigitNumber, DecimalDigitNumber, DecimalDigitNumber, DecimalDigitNumber, DecimalDigitNumber, DecimalDigitNumber, DecimalDigitNumber, // 0030 - 0037
					DecimalDigitNumber, DecimalDigitNumber, OtherPunctuation, OtherPunctuation, MathSymbol, MathSymbol, MathSymbol, OtherPunctuation, // 0038 - 003F
					OtherPunctuation, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, // 0040 - 0047
					UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, // 0048 - 004F
					UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, // 0050 - 0057
					UppercaseLetter, UppercaseLetter, UppercaseLetter, OpenPunctuation, OtherPunctuation, ClosePunctuation, ModifierSymbol, ConnectorPunctuation, // 0058 - 005F
					ModifierSymbol, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, // 0060 - 0067
					LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, // 0068 - 006F
					LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, // 0070 - 0077
					LowercaseLetter, LowercaseLetter, LowercaseLetter, OpenPunctuation, MathSymbol, ClosePunctuation, MathSymbol, Control, // 0078 - 007F
					Control, Control, Control, Control, Control, Control, Control, Control, // 0080 - 0087
					Control, Control, Control, Control, Control, Control, Control, Control, // 0088 - 008F
					Control, Control, Control, Control, Control, Control, Control, Control, // 0090 - 0097
					Control, Control, Control, Control, Control, Control, Control, Control, // 0098 - 009F
					SpaceSeparator, OtherPunctuation, CurrencySymbol, CurrencySymbol, CurrencySymbol, CurrencySymbol, OtherSymbol, OtherSymbol, // 00A0 - 00A7
					ModifierSymbol, OtherSymbol, LowercaseLetter, InitialQuotePunctuation, MathSymbol, DashPunctuation, OtherSymbol, ModifierSymbol, // 00A8 - 00AF 
					OtherSymbol, MathSymbol, OtherNumber, OtherNumber, ModifierSymbol, LowercaseLetter, OtherSymbol, OtherPunctuation, // 00B0 - 00B7
					ModifierSymbol, OtherNumber, LowercaseLetter, FinalQuotePunctuation, OtherNumber, OtherNumber, OtherNumber, OtherPunctuation, // 00B8 - 00BF
					UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, // 00C0 - 00C7
					UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, // 00C8 - 00CF
					UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, MathSymbol, // 00D0 - 00D7
					UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, UppercaseLetter, LowercaseLetter, // 00D8 - 00DF
					LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, // 00E0 - 00E7
					LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, // 00E8 - 00EF
					LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, MathSymbol, // 00F0 - 00F7
					LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter, LowercaseLetter // 00F8 - 00FF
				};

				return latin1Category[(unsigned char)c];
			}

			/**
			 * Ruft ab, ob das Zeichen zum ASCII Zeichensatz gehört.
			 *
			 * \param c
			 * \return
			 */
			inline bool IsAscii(const AnsiChar c)
			{
				return c <= '\x7F';
			}

			/**
			 * Ruft ab, ob das Zeichen eine Zahl ist.
			 *
			 * \param c
			 * \return
			 */
			inline bool IsDigit(const AnsiChar c)
			{
				if (IsAscii(c))
				{
					return c >= '0' && c <= '9';
				}
				return GetLatin1Category(c) == DecimalDigitNumber;
			}

			/**
			 * Ruft ab, ob das Zeichen eine Nummer ist.
			 *
			 * \param c
			 * \return
			 */
			inline bool IsNumber(const AnsiChar c)
			{
				if (IsAscii(c))
				{
					return c >= '0' && c <= '9';
				}
				switch (GetLatin1Category(c))
				{
					case DecimalDigitNumber:
					case OtherNumber:
						return true;
				}
				return false;
			}

			/**
			 * Ruft ab, ob das Zeichen eine druckbare Ziffer ist.
			 *
			 * \param c
			 * \return
			 */
			inline bool IsLetter(const AnsiChar c)
			{
				if (IsAscii(c))
				{
					AnsiChar x = c | (char)0x20;
					return (x >= 'a' && x <= 'z');
				}
				switch (GetLatin1Category(c))
				{
					case UppercaseLetter:
					case LowercaseLetter:
						return true;
				}
				return false;
			}

			/**
			 * Ruft ab, ob das Zeichen ein Whitespace ist.
			 *
			 * \param c
			 * \return
			 */
			inline bool IsWhiteSpace(const AnsiChar c)
			{
				return ((c == ' ') || (c >= '\x09' && c <= '\x0d') || c == '\xa0' || c == '\x85');
			}

			/**
			 * Ruft ab, ob das Zeichen ein Großbuchstabe ist.
			 *
			 * \param c
			 * \return
			 */
			inline bool IsUpper(const AnsiChar c)
			{
				if (IsAscii(c))
				{
					return c >= 'A' && c <= 'Z';
				}
				return GetLatin1Category(c) == UppercaseLetter;
			}

			/**
			 * Ruft ab, ob das Zeichen ein Kleinbuchstabe ist.
			 *
			 * \param c
			 * \return
			 */
			inline bool IsLower(const AnsiChar c)
			{
				if (IsAscii(c))
				{
					return c >= 'a' && c <= 'z';
				}
				return GetLatin1Category(c) == LowercaseLetter;
			}

			/**
			 * Ruft ab, ob das Zeichen ein Symbol ist.
			 *
			 * \param c
			 * \return
			 */
			inline bool IsSymbol(const AnsiChar c)
			{
				switch (GetLatin1Category(c))
				{
					case OtherSymbol:
					case ModifierSymbol:
					case MathSymbol:
					case CurrencySymbol:
						return true;
				}
				return false;
			}

			/**
			 * Ruft ab, ob das Zeichen eine Punktion ist.
			 *
			 * \param c
			 * \return
			 */
			inline bool IsPunctuation(const AnsiChar c)
			{
				switch (GetLatin1Category(c))
				{
					case ConnectorPunctuation:
					case DashPunctuation:
					case OpenPunctuation:
					case ClosePunctuation:
					case InitialQuotePunctuation:
					case FinalQuotePunctuation:
					case OtherPunctuation:
						return true;
				} 
				return false;
			}

			/**
			 * Ruft ab, ob das Zeichen ein Seperator ist.
			 *
			 * \param c
			 * \return
			 */
			inline bool IsSeperator(const AnsiChar c)
			{
				return c == '\x20' || c == '\xa0';
			}

			/**
			 * Ruft ab, ob das Zeichen eine Ziffer oder Zahl ist.
			 *
			 * \param c
			 * \return
			 */
			inline bool IsLetterOrDigit(const AnsiChar c)
			{
				switch (GetLatin1Category(c))
				{
					case UppercaseLetter:
					case LowercaseLetter:
					case DecimalDigitNumber:
						return true;
				} 
				return false;
			}

			/**
			 * Ruft ab, ob das Zeichen ein Steuersymbol ist.
			 *
			 * \param c
			 * \return
			 */
			inline bool IsControl(const AnsiChar c)
			{
				return GetLatin1Category(c) == Control;
			}

			/**
			 * Wandelt einen AnsiString in einen UnicodeString um.
			 *
			 * \param ansi
			 * \return unicode
			 */
			inline UnicodeString AnsiToUnicode(const AnsiString &ansi)
			{
				if (ansi.length() == 0)
				{
					return L"";
				}

				UnicodeString out(ansi.length(), (UnicodeChar)0);

				std::use_facet<std::ctype<UnicodeChar>>(std::locale()).widen(&ansi[0], &ansi[0] + ansi.length(), &out[0]);

				return out;
			}
			/**
			 * Wandelt einen UnicodeString in einen AnsiString um.
			 *
			 * \param unicode
			 * \return ansi
			 */
			inline AnsiString UnicodeToAnsi(const UnicodeString &unicode)
			{
				if (unicode.length() == 0)
				{
					return "";
				}

				AnsiString out(unicode.length(), (AnsiChar)0);

				std::use_facet<std::ctype<UnicodeChar>>(std::locale()).narrow(&unicode[0], &unicode[0] + unicode.length(), '?', &out[0]);

				return out;
			}
			
			/**
			 * Ersetzt das Formatelement in einer angegebenen Unicode-Zeichenfolge durch die
			 * Zeichenfolgendarstellung eines entsprechenden Objekts.
			 *
			 * \param *fmt
			 * \param ...
			 * \return UnicodeString
			 */
			UnicodeString Format(const UnicodeChar *fmt, ...);
			/**
			 * Ersetzt das Formatelement in einer angegebenen Ansi-Zeichenfolge durch die
			 * Zeichenfolgendarstellung eines entsprechenden Objekts.
			 *
			 * \param *fmt
			 * \param ...
			 * \return AnsiString
			 */
			AnsiString Format(const AnsiChar *fmt, ...);
		}
	}
}

#endif