#include "Direct3D11ViewportTarget.hpp"
#include "../RenderQueue.hpp"
#include "../GeometryBuffer.hpp"
#include "../../Misc/Exceptions.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D11ViewportTarget::Direct3D11ViewportTarget(Direct3D11Renderer &owner)
			: Direct3D11RenderTarget(owner)
		{
			D3D11_VIEWPORT vp;
			uint32_t count = 1;
			owner.GetDevice().Context->RSGetViewports(&count, &vp);
			
			if (count != 1)
			{
				throw Misc::Exception();
			}

			SetArea(RectangleF(PointF(vp.TopLeftX, vp.TopLeftY), SizeF(vp.Width, vp.Height)));
		}
		//---------------------------------------------------------------------------
		Direct3D11ViewportTarget::Direct3D11ViewportTarget(Direct3D11Renderer &owner, const RectangleF &area)
			: Direct3D11RenderTarget(owner)
		{
			SetArea(area);
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		bool Direct3D11ViewportTarget::IsImageryCache() const
		{
			return false;
		}
		//---------------------------------------------------------------------------
	}
}
