/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_RENDERCONTEXT_HPP
#define OSHGUI_DRAWING_RENDERCONTEXT_HPP

#include "Point.hpp"
#include "RenderSurface.hpp"

namespace OSHGui
{
	class Control;

	namespace Drawing
	{
		class OSHGUI_EXPORT RenderContext
		{
		public:
			RenderSurface *Surface;
			const Control *Owner;
			PointF Offset;
			RenderQueueType QueueType;
		};
	}
}

#endif
