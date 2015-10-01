/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_DIRECT3D8VIEWPORTTARGET_HPP
#define OSHGUI_DRAWING_DIRECT3D8VIEWPORTTARGET_HPP

#include "Direct3D8RenderTarget.hpp"
#include "../Rectangle.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		/**
		 * Die Direct3D8 Variante des RenderTarget.
		 */
		class OSHGUI_EXPORT Direct3D8ViewportTarget : public Direct3D8RenderTarget<>
		{
		public:
			/**
			 * Erzeugt ein RenderTarget, das den Direct3D8 Viewport für Zeichenoperationen verwendet.
			 *
			 * \param owner Renderer Objekt, das das RenderTarget erstellt hat
			 */
			Direct3D8ViewportTarget(Direct3D8Renderer &owner);

			/**
			 * Erzeugt ein RenderTarget, das den angegebenen Bereich für Zeichenoperationen verwendet.
			 *
			 * \param owner Renderer Objekt, das das RenderTarget erstellt hat
			 * \param area Bereich für Zeichenoperationen
			 */
			Direct3D8ViewportTarget(Direct3D8Renderer &owner, const RectangleF &area);

			virtual bool IsImageryCache() const override;
		};
	}

}

#endif