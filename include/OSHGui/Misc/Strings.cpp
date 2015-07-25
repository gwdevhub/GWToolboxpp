/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "Strings.hpp"
#include <algorithm>

namespace OSHGui
{
	namespace Misc
	{
		namespace String
		{
			UnicodeString Format(const UnicodeChar *fmt, ...)
			{
				UnicodeChar strOut[4096];

				va_list s;
				va_start(s, fmt);
					vswprintf(strOut, sizeof(strOut) / sizeof(UnicodeChar), fmt, s);
				va_end(s);

				UnicodeString str = strOut;
				return str;
			}
			//---------------------------------------------------------------------------
			AnsiString Format(const AnsiChar *fmt, ...)
			{
				AnsiChar strOut[4096];

				va_list s;
				va_start(s, fmt);
					vsnprintf_s(strOut, sizeof(strOut) / sizeof(AnsiChar), fmt, s);
				va_end(s);

				AnsiString str = strOut;
				return str;
			}
			//---------------------------------------------------------------------------
		}
	}
}