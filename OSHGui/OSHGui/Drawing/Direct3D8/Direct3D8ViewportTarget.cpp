#include "Direct3D8ViewportTarget.hpp"
#include "../RenderQueue.hpp"
#include "../GeometryBuffer.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D8ViewportTarget::Direct3D8ViewportTarget(Direct3D8Renderer &owner)
			: Direct3D8RenderTarget(owner)
		{
			D3DVIEWPORT8 vp;
			if (FAILED(owner.GetDevice()->GetViewport(&vp)))
			{
				throw;
			}

			SetArea(RectangleF(PointF(vp.X, vp.Y), SizeF(vp.Width, vp.Height)));
		}
		//---------------------------------------------------------------------------
		Direct3D8ViewportTarget::Direct3D8ViewportTarget(Direct3D8Renderer &owner, const RectangleF &area)
			: Direct3D8RenderTarget(owner)
		{
			SetArea(area);
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		bool Direct3D8ViewportTarget::IsImageryCache() const
		{
			return false;
		}
		//---------------------------------------------------------------------------
	}
}
