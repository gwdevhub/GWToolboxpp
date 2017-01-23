/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_DIRECT3D11VIEWPORTTARGET_HPP
#define OSHGUI_DRAWING_DIRECT3D11VIEWPORTTARGET_HPP

#include "Direct3D11RenderTarget.hpp"
#include "../Rectangle.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		/**
		 * Die Direct3D11 Variante des RenderTarget.
		 */
		class OSHGUI_EXPORT Direct3D11ViewportTarget : public Direct3D11RenderTarget<>
		{
		public:
			/**
			 * Erzeugt ein RenderTarget, das den Direct3D11 Viewport für Zeichenoperationen verwendet.
			 *
			 * \param owner Renderer Objekt, das das RenderTarget erstellt hat
			 */
			Direct3D11ViewportTarget(Direct3D11Renderer &owner);

			/**
			 * Erzeugt ein RenderTarget, das den angegebenen Bereich für Zeichenoperationen verwendet.
			 *
			 * \param owner Renderer Objekt, das das RenderTarget erstellt hat
			 * \param area Bereich für Zeichenoperationen
			 */
			Direct3D11ViewportTarget(Direct3D11Renderer &owner, const RectangleF &area);

			virtual bool IsImageryCache() const override;
		};
	}

}

#endif