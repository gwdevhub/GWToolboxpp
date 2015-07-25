/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_OPENGLVIEWPORTTARGET_HPP
#define OSHGUI_DRAWING_OPENGLVIEWPORTTARGET_HPP

#include "OpenGLRenderTarget.hpp"
#include "../Rectangle.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		/**
		 * Die OpenGL Variante des RenderTarget.
		 */
		class OSHGUI_EXPORT OpenGLViewportTarget : public OpenGLRenderTarget<>
		{
		public:
			/**
			 * Erzeugt ein RenderTarget, das den OpenGL Viewport für Zeichenoperationen verwendet.
			 *
			 * \param owner Renderer Objekt, das das RenderTarget erstellt hat
			 */
			OpenGLViewportTarget(OpenGLRenderer &owner);

			/**
			 * Erzeugt ein RenderTarget, das den angegebenen Bereich für Zeichenoperationen verwendet.
			 *
			 * \param owner Renderer Objekt, das das RenderTarget erstellt hat
			 * \param area Bereich für Zeichenoperationen
			 */
			OpenGLViewportTarget(OpenGLRenderer &owner, const RectangleF &area);

			virtual bool IsImageryCache() const override;
		};
	}

}

#endif