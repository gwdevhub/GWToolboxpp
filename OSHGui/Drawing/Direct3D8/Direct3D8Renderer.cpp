#include "Direct3D8Renderer.hpp"
#include "Direct3D8Texture.hpp"
#include "Direct3D8GeometryBuffer.hpp"
#include "Direct3D8RenderTarget.hpp"
#include "Direct3D8ViewportTarget.hpp"
#include "Direct3D8TextureTarget.hpp"
#include "../../Misc/Exceptions.hpp"

#include <algorithm>

namespace OSHGui
{
	namespace Drawing
	{
		static const D3DMATRIX Identity =
		{
			1.0, 0.0, 0.0, 0.0,
			0.0, 1.0, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			0.0, 0.0, 0.0, 1.0
		};
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D8Renderer::Direct3D8Renderer(LPDIRECT3DDEVICE8 _device)
			: device(_device),
			  displaySize(GetViewportSize()),
			  displayDPI(96, 96)
		{
			D3DCAPS8 caps;
			device->GetDeviceCaps(&caps);

			maxTextureSize = std::min(caps.MaxTextureHeight, caps.MaxTextureWidth);

			supportNonSquareTex = !(caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY);

			supportNPOTTex = !(caps.TextureCaps & D3DPTEXTURECAPS_POW2) || (caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL);

			defaultTarget = std::make_shared<Direct3D8ViewportTarget>(*this);

			PostD3DReset(); //indirect initialize
		}
		//---------------------------------------------------------------------------
		Direct3D8Renderer::~Direct3D8Renderer()
		{
			if (stateBlockHandle)
			{
				device->DeleteStateBlock(stateBlockHandle);
			}
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		RenderTargetPtr& Direct3D8Renderer::GetDefaultRenderTarget()
		{
			return defaultTarget;
		}
		//---------------------------------------------------------------------------
		void Direct3D8Renderer::SetDisplaySize(const SizeF &size)
		{
			if (size != displaySize)
			{
				displaySize = size;

				auto area = defaultTarget->GetArea();
				area.SetSize(size);
				defaultTarget->SetArea(area);
			}
		}
		//---------------------------------------------------------------------------
		const SizeF& Direct3D8Renderer::GetDisplaySize() const
		{
			return displaySize;
		}
		//---------------------------------------------------------------------------
		const PointF& Direct3D8Renderer::GetDisplayDPI() const
		{
			return displayDPI;
		}
		//---------------------------------------------------------------------------
		uint32_t Direct3D8Renderer::GetMaximumTextureSize() const
		{
			return maxTextureSize;
		}
		//---------------------------------------------------------------------------
		SizeF Direct3D8Renderer::GetViewportSize()
		{
			D3DVIEWPORT8 vp;
			if (FAILED(device->GetViewport(&vp)))
			{
				throw Misc::Exception();
			}

			return SizeF(vp.Width, vp.Height);
		}
		//---------------------------------------------------------------------------
		LPDIRECT3DDEVICE8 Direct3D8Renderer::GetDevice() const
		{
			return device;
		}
		//---------------------------------------------------------------------------
		bool Direct3D8Renderer::SupportsNonSquareTexture()
		{
			return supportNonSquareTex;
		}
		//---------------------------------------------------------------------------
		bool Direct3D8Renderer::SupportsNPOTTextures()
		{
			return supportNPOTTex;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		GeometryBufferPtr Direct3D8Renderer::CreateGeometryBuffer()
		{
			return std::make_shared<Direct3D8GeometryBuffer>(*this);
		}
		//---------------------------------------------------------------------------
		TextureTargetPtr Direct3D8Renderer::CreateTextureTarget()
		{
			auto textureTarget = std::make_shared<Direct3D8TextureTarget>(*this);
			textureTargets.emplace_back(textureTarget);
			return textureTarget;
		}
		//---------------------------------------------------------------------------
		TexturePtr Direct3D8Renderer::CreateTexture()
		{
			auto texture = std::shared_ptr<Direct3D8Texture>(new Direct3D8Texture(*this));
			textures.emplace_back(texture);
			return texture;
		}
		//---------------------------------------------------------------------------
		TexturePtr Direct3D8Renderer::CreateTexture(const Misc::AnsiString &filename)
		{
			auto texture = std::shared_ptr<Direct3D8Texture>(new Direct3D8Texture(*this, filename));
			textures.emplace_back(texture);
			return texture;
		}
		//---------------------------------------------------------------------------
		TexturePtr Direct3D8Renderer::CreateTexture(const SizeF &size)
		{
			auto texture = std::shared_ptr<Direct3D8Texture>(new Direct3D8Texture(*this, size));
			textures.emplace_back(texture);
			return texture;
		}
		//---------------------------------------------------------------------------
		void Direct3D8Renderer::BeginRendering()
		{
			device->CaptureStateBlock(stateBlockHandle);

			device->SetVertexShader(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
			device->SetPixelShader(0);

			device->SetRenderState(D3DRS_LIGHTING, FALSE);
			device->SetRenderState(D3DRS_FOGENABLE, FALSE);
			device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
			device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
			device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
			device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
			device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
			device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);

			device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);

			device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
			device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
			device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

			device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

			device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

			device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

			device->SetTransform(D3DTS_VIEW, &Identity);
		}
		//---------------------------------------------------------------------------
		void Direct3D8Renderer::EndRendering()
		{
			device->ApplyStateBlock(stateBlockHandle);
		}
		//---------------------------------------------------------------------------
		void Direct3D8Renderer::PreD3DReset()
		{
			device->DeleteStateBlock(stateBlockHandle);

			RemoveWeakReferences();

			for (auto &textureTarget : textureTargets)
			{
				textureTarget.lock()->PreD3DReset();
			}
			for (auto &texture : textures)
			{
				texture.lock()->PreD3DReset();
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D8Renderer::PostD3DReset()
		{
			device->CreateStateBlock(D3DSBT_ALL, &stateBlockHandle);

			RemoveWeakReferences();

			for (auto &textureTarget : textureTargets)
			{
				textureTarget.lock()->PostD3DReset();
			}
			for (auto &texture : textures)
			{
				texture.lock()->PostD3DReset();
			}
		}
		//---------------------------------------------------------------------------
		SizeF Direct3D8Renderer::GetAdjustedSize(const SizeF &sz)
		{
			auto s(sz);

			if (!supportNPOTTex)
			{
				s.Width  = GetSizeNextPOT(sz.Width);
				s.Height = GetSizeNextPOT(sz.Height);
			}
			if (!supportNonSquareTex)
			{
				s.Width = s.Height = std::max(s.Width, s.Height);
			}

			return s;
		}
		//---------------------------------------------------------------------------
		float Direct3D8Renderer::GetSizeNextPOT(float _size) const
		{
			uint32_t size = _size;

			if ((size & (size - 1)) || !size)
			{
				int log = 0;

				while (size >>= 1)
				{
					++log;
				}

				size = (2 << log);
			}

			return size;
		}
		//---------------------------------------------------------------------------
		void Direct3D8Renderer::RemoveWeakReferences()
		{
			textureTargets.erase(std::remove_if(std::begin(textureTargets), std::end(textureTargets), [](const std::weak_ptr<Direct3D8TextureTarget> ptr) { return ptr.expired(); }), std::end(textureTargets));
			textures.erase(std::remove_if(std::begin(textures), std::end(textures), [](const std::weak_ptr<Direct3D8Texture> ptr) { return ptr.expired(); }), std::end(textures));
		}
		//---------------------------------------------------------------------------
	}
}