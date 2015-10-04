#include "Direct3D7ViewportTarget.hpp"
#include "../RenderQueue.hpp"
#include "../GeometryBuffer.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D7ViewportTarget::Direct3D7ViewportTarget(Direct3D7Renderer &owner)
			: Direct3D7RenderTarget(owner)
		{
			D3DVIEWPORT7 vp;
			if (FAILED(owner.GetDevice().Device->GetViewport(&vp)))
			{
				throw;
			}

			SetArea(RectangleF(PointF(vp.dwX, vp.dwY), SizeF(vp.dwWidth, vp.dwHeight)));
		}
		//---------------------------------------------------------------------------
		Direct3D7ViewportTarget::Direct3D7ViewportTarget(Direct3D7Renderer &owner, const RectangleF &area)
			: Direct3D7RenderTarget(owner)
		{
			SetArea(area);
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		bool Direct3D7ViewportTarget::IsImageryCache() const
		{
			return false;
		}
		//---------------------------------------------------------------------------
	}
}
