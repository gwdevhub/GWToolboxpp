/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_RENDERSURFACE_HPP
#define OSHGUI_DRAWING_RENDERSURFACE_HPP

#include "../Exports.hpp"
#include "RenderQueue.hpp"
#include "Point.hpp"
#include <vector>

namespace OSHGui
{
	namespace Drawing
	{
		class RenderTarget;

		enum class RenderQueueType
		{
			Underlay,
			Base,
			Overlay
		};

		class OSHGUI_EXPORT RenderSurface
		{
		public:
			RenderSurface(RenderTarget &target);
			virtual ~RenderSurface() { }

			virtual void AddGeometry(RenderQueueType type, const GeometryBufferPtr &geometry);
			virtual void RemoveGeometry(RenderQueueType type, const GeometryBufferPtr &geometry);
			virtual void ResetQueue(RenderQueueType type);
			virtual void Reset();

			virtual void Draw();

		private:
			RenderTarget &target;

			typedef std::vector<RenderQueue> RenderQueueList;
			RenderQueueList queue;
		};
	}
}

#endif
