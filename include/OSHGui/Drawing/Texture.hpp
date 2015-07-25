/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_TEXTURE_HPP
#define OSHGUI_DRAWING_TEXTURE_HPP

#include "../Exports.hpp"
#include "Size.hpp"
#include "Point.hpp"
#include "Rectangle.hpp"
#include "Vertex.hpp"
#include "../Misc/Strings.hpp"
#include <memory>

namespace OSHGui
{
	namespace Drawing
	{
		/**
		 * Die abstrakte Klasse repräsentiert eine Textur und wird durch den Renderer erstellt.
		 */
		class OSHGUI_EXPORT Texture
		{
		public:
			/**
			 * Auflistung der möglichen PixelFormate.
			 */
			enum class PixelFormat
			{
				RGB,
				RGBA,
				RGBA_4444,
				RGB_565,
				PVRTC2,
				PVRTC4,
				RGB_DXT1,
				RGBA_DXT1,
				RGBA_DXT3,
				RGBA_DXT5
			};

			/**
			 * Destructor der Klasse.
			 */
			virtual ~Texture()
			{

			}

			/**
			 * Ruft die (eventuell gepaddete) Größe der Textur in Pixeln ab.
			 *
			 * \return Größe der Textur
			 */
			virtual const SizeF& GetSize() const = 0;

			/**
			 * Ruft die Originalgröße der Daten der Textur ab, mit denen sie erstellt wurde.
			 *
			 * \return Originalgröße der Textur
			 */
			virtual const SizeF& GetOriginalDataSize() const = 0;

			/**
			 * Ruft die Pixel-zu-Texel Werte ab, die benutzt werden um Pixelwerte zu Texturkoordinaten umzurechnen.
			 *
			 * \return Pixel-zu-Texel Werte
			 */
			virtual const std::pair<float, float>& GetTexelScaling() const = 0;

			/**
			 * Lädt die Textur aus einer Datei.
			 *
			 * \param path Pfad zur Datei
			 */
			virtual void LoadFromFile(const Misc::AnsiString &path) = 0;

			/**
			 * Lädt die Textur aus dem Speicher. Die Daten im Buffer müssen im angegebenen Pixelformat vorliegen.
			 *
			 * \param buffer
			 * \param size die Größe des Bildes
			 * \param format
			 */
			virtual void LoadFromMemory(const void *buffer, const SizeF &size, PixelFormat format) = 0;

			/**
			 * Prüft, ob Texturen im angegebenen Format erstellt werden können.
			 *
			 * \param format das PixelFormat
			 * \return true falls das Format unterstützt wird
			 */
			virtual bool IsPixelFormatSupported(const PixelFormat format) const = 0;
		};

		typedef std::shared_ptr<Texture> TexturePtr;
	}
}

#endif