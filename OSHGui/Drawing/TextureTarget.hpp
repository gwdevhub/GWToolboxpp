/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_TEXTURETARGET_HPP
#define OSHGUI_DRAWING_TEXTURETARGET_HPP

#include "RenderTarget.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		/**
		 * Spezielles RenderTarget, das ein Interface für ein TextureTarget bereitstellt.
		 */
		class OSHGUI_EXPORT TextureTarget : public RenderTarget
		{
		public:
			/**
			 * Ruft die Textur ab, die als TextureTarget verwendet wird.
			 *
			 * \return TextureTarget
			 */
			virtual TexturePtr GetTexture() const = 0;

			/**
			 * Legt die Größe des Zeichenbereichs fest.
			 *
			 * \param size Größe des Zeichenbereichs
			 */
			virtual void DeclareRenderSize(const SizeF& size) = 0;

			virtual bool IsRenderingInverted() const = 0;

			/**
			 * Löscht den Inhalt der verwendeten Textur.
			 */
			virtual void Clear() = 0;
		};

		typedef std::shared_ptr<TextureTarget> TextureTargetPtr;
	}
}

#endif