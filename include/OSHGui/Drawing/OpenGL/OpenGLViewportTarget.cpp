#include "OpenGLViewportTarget.hpp"
#include "../RenderQueue.hpp"
#include "../GeometryBuffer.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		OpenGLViewportTarget::OpenGLViewportTarget(OpenGLRenderer &owner)
			: OpenGLRenderTarget(owner)
		{
			GLint vp[4];
			glGetIntegerv(GL_VIEWPORT, vp);

			SetArea(RectangleF(PointF(vp[0], vp[1]), SizeF(vp[2], vp[3])));
		}
		//---------------------------------------------------------------------------
		OpenGLViewportTarget::OpenGLViewportTarget(OpenGLRenderer &owner, const RectangleF &area)
			: OpenGLRenderTarget(owner)
		{
			SetArea(area);
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		bool OpenGLViewportTarget::IsImageryCache() const
		{
			return false;
		}
		//---------------------------------------------------------------------------
	}
}
