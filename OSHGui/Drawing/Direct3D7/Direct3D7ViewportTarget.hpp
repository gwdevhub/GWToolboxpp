/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_DIRECT3D7VIEWPORTTARGET_HPP
#define OSHGUI_DRAWING_DIRECT3D7VIEWPORTTARGET_HPP

#include "Direct3D7RenderTarget.hpp"
#include "../Rectangle.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		/**
		 * Die Direct3D7 Variante des RenderTarget.
		 */
		class OSHGUI_EXPORT Direct3D7ViewportTarget : public Direct3D7RenderTarget<>
		{
		public:
			/**
			 * Erzeugt ein RenderTarget, das den Direct3D7 Viewport für Zeichenoperationen verwendet.
			 *
			 * \param owner Renderer Objekt, das das RenderTarget erstellt hat
			 */
			Direct3D7ViewportTarget(Direct3D7Renderer &owner);

			/**
			 * Erzeugt ein RenderTarget, das den angegebenen Bereich für Zeichenoperationen verwendet.
			 *
			 * \param owner Renderer Objekt, das das RenderTarget erstellt hat
			 * \param area Bereich für Zeichenoperationen
			 */
			Direct3D7ViewportTarget(Direct3D7Renderer &owner, const RectangleF &area);

			virtual bool IsImageryCache() const override;
		};
	}

}

#endif