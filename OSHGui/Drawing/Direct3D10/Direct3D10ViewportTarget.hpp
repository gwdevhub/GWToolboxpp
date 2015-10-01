/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_DIRECT3D10VIEWPORTTARGET_HPP
#define OSHGUI_DRAWING_DIRECT3D10VIEWPORTTARGET_HPP

#include "Direct3D10RenderTarget.hpp"
#include "../Rectangle.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		/**
		 * Die Direct3D10 Variante des RenderTarget.
		 */
		class OSHGUI_EXPORT Direct3D10ViewportTarget : public Direct3D10RenderTarget<>
		{
		public:
			/**
			 * Erzeugt ein RenderTarget, das den Direct3D10 Viewport für Zeichenoperationen verwendet.
			 *
			 * \param owner Renderer Objekt, das das RenderTarget erstellt hat
			 */
			Direct3D10ViewportTarget(Direct3D10Renderer &owner);

			/**
			 * Erzeugt ein RenderTarget, das den angegebenen Bereich für Zeichenoperationen verwendet.
			 *
			 * \param owner Renderer Objekt, das das RenderTarget erstellt hat
			 * \param area Bereich für Zeichenoperationen
			 */
			Direct3D10ViewportTarget(Direct3D10Renderer &owner, const RectangleF &area);

			virtual bool IsImageryCache() const override;
		};
	}

}

#endif