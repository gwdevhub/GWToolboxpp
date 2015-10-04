#ifndef OSHGUI_THEME_HPP
#define OSHGUI_THEME_HPP

#include "../Exports.hpp"
#include "Color.hpp"
#include "../Misc/Strings.hpp"
#include <map>

namespace OSHGui
{
	namespace Drawing
	{
		class OSHGUI_EXPORT Theme
		{
		public:
			struct ControlTheme
			{
				ControlTheme()
				{

				}

				ControlTheme(const Color &foreColor, const Color &backColor)
					: ForeColor(foreColor),
					  BackColor(backColor)
				{

				}

				Color ForeColor;
				Color BackColor;
			};
		
			enum ColorStyle
			{
				Text,
				Array,
				Integer
			};

			Misc::AnsiString Name;
			Misc::AnsiString Author;
			ControlTheme DefaultColor;

			/**
			 * Legt das Farbschema für eine Control Klasse fest. Die Klassennamen sind in der Control.hpp zu finden.
			 *
			 * \param controlClass
			 * \param controlTheme
			 */
			void SetControlColorTheme(const Misc::AnsiString &controlClass, const ControlTheme &controlTheme);
			/**
			 * Ruft das Farbschema für eine Control Klasse ab. Die Klassennamen sind in der Control.hpp zu finden.
			 *
			 * \param controlClass
			 * \return controlTheme
			 */
			const ControlTheme& GetControlColorTheme(const Misc::AnsiString &controlClass) const;

			/**
			 * Lädt ein Theme vom angegebenen Dateipfad. Im Fehlerfall wird eine InvalidThemeException geworfen.
			 *
			 * \param pathToThemeFile
			 */
			void Load(const Misc::UnicodeString &pathToThemeFile);
			/**
			 * Speichert ein Theme am angegebenen Dateipfad.
			 *
			 * \param pathToThemeFile
			 * \param style Array, Text, Int
			 * \return im Fehlerfall false
			 */
			bool Save(const Misc::UnicodeString &pathToThemeFile, ColorStyle style) const;

		private:
			std::map<Misc::AnsiString, ControlTheme> controlThemes;
		};
	}
}

#endif

/* Theme File Style

1) Array Style
{
	"name" : "Test Theme",
	"author" : "KN4CK3R",
	"default" : {
		"forecolor" : [ 0, 0, 0, 0 ],
		"backcolor" : [ 0, 0, 0, 0 ]
	},
	"themes" : {
		"button" : {
			"forecolor" : [ 2, 3, 4, 1 ],
			"backcolor" : [ 5, 6, 7, 4 ]
		},
		"label" : {
			"forecolor" : [ 2, 3, 4, 1 ],
			"backcolor" : [ 5, 6, 7, 4 ]
		},
		"linklabel" : {
			"forecolor" : [ 2, 3, 4, 1 ],
			"backcolor" : [ 5, 6, 7, 4 ]
		}
	}
}
 
2) Text Style
{
	"name" : "New Test Theme",
	"author" : "KN4CK3R",
	"default" : {
		"forecolor" : {
			"r" : 0,
			"g" : 0,
			"b" : 0,
			"a" : 0
		},
		"backcolor" : {
			"r" : 0,
			"g" : 0,
			"b" : 0,
			"a" : 0
		}
	},
	"themes" : {
		"button" : {
			"forecolor" : {
				"r" : 2,
				"g" : 3,
				"b" : 4,
				"a" : 1
			},
			"backcolor" : {
				"r" : 5,
				"g" : 6,
				"b" : 7,
				"a" : 4
			}
		},
		"label" : {
			"forecolor" : {
				"r" : 2,
				"g" : 3,
				"b" : 4,
				"a" : 1
			},
			"backcolor" : {
				"r" : 5,
				"g" : 6,
				"b" : 7,
				"a" : 4
			}
		},
		"linklabel" : {
			"forecolor" : {
				"r" : 2,
				"g" : 3,
				"b" : 4,
				"a" : 1
			},
			"backcolor" : {
				"r" : 5,
				"g" : 6,
				"b" : 7,
				"a" : 4
			}
		}
	}
}
 
3) Hex Number Style
{
	"name" : "Test Theme",
	"author" : "KN4CK3R",
	"default" : {
		"forecolor" : "00000000",
		"backcolor" : "00000000"
	},
	"themes" : {
		"button" : {
			"forecolor" : "01020304",
			"backcolor" : "04050607"
		},
		"label" : {
			"forecolor" : "01020304",
			"backcolor" : "04050607"
		},
		"linklabel" : {
			"forecolor" : "01020304",
			"backcolor" : "04050607"
		}
	}
}*/