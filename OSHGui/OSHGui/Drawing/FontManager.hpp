/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_FONTMANAGER_HPP
#define OSHGUI_DRAWING_FONTMANAGER_HPP

#include <unordered_map>
#include "../Exports.hpp"
#include "Font.hpp"
#include "../Misc/RawDataContainer.hpp"

namespace OSHGui {
	namespace Drawing {
		/**
		 * The class allows loading a font.
		 */
		class OSHGUI_EXPORT FontManager {
		public:
			/**
			 * Loads the font with the corresponding name.
			 *
			 * \param name of the font
			 * \param pointSize Size in PT
			 * \param antiAliased Determines whether antialiasing should be used
			 * \return The downloaded font or nullptr, if the font is not found.
			 */
			static FontPtr LoadFont(Misc::AnsiString name, float pointSize, bool antiAliased);
			/**
			 * Loads the font from the specified file.
			 *
			 * \param file Path to the file
			 * \param pointSize Size in PT
			 * \param antiAliased Determines whether antialiasing should be used
			 * \return The downloaded font or nullptr, if the font is not found.
			 */
			static FontPtr LoadFontFromFile(const Misc::AnsiString &file, float pointSize, bool antiAliased);
			static FontPtr LoadFontFromMemory(const Misc::RawDataContainer &data, float pointSize, bool antiAliased);

			/**
			* Specifies the screen size.
			*
			* @param size
			*/
			static void DisplaySizeChanged(const SizeI &size);

		private:
			static std::unordered_map<Misc::AnsiString, std::weak_ptr<Drawing::Font>> loadedFonts;
		};
	}
}

#endif