#include "Direct3D9Renderer.hpp"
#include "Direct3D9Texture.hpp"
#include "Direct3D9GeometryBuffer.hpp"
#include "Direct3D9RenderTarget.hpp"
#include "Direct3D9ViewportTarget.hpp"
#include "Direct3D9TextureTarget.hpp"
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
		Direct3D9Renderer::Direct3D9Renderer(LPDIRECT3DDEVICE9 _device)
			: device(_device),
			  displaySize(GetViewportSize()),
			  displayDPI(96, 96),
			  stateBlock(nullptr)
		{
			D3DCAPS9 caps;
			device->GetDeviceCaps(&caps);

			if (!caps.RasterCaps && D3DPRASTERCAPS_SCISSORTEST)
			{
				throw Misc::NotSupportedException();
			}

			maxTextureSize = std::min(caps.MaxTextureHeight, caps.MaxTextureWidth);

			supportNonSquareTex = !(caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY);

			supportNPOTTex = !(caps.TextureCaps & D3DPTEXTURECAPS_POW2) || (caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL);

			defaultTarget = std::make_shared<Direct3D9ViewportTarget>(*this);

			PostD3DReset(); //indirect initialize
		}
		//---------------------------------------------------------------------------
		Direct3D9Renderer::~Direct3D9Renderer()
		{
			if (stateBlock)
			{
				stateBlock->Release();
			}
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		RenderTargetPtr& Direct3D9Renderer::GetDefaultRenderTarget()
		{
			return defaultTarget;
		}
		//---------------------------------------------------------------------------
		void Direct3D9Renderer::SetDisplaySize(const SizeF &size)
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
		const SizeF& Direct3D9Renderer::GetDisplaySize() const
		{
			return displaySize;
		}
		//---------------------------------------------------------------------------
		const PointF& Direct3D9Renderer::GetDisplayDPI() const
		{
			return displayDPI;
		}
		//---------------------------------------------------------------------------
		uint32_t Direct3D9Renderer::GetMaximumTextureSize() const
		{
			return maxTextureSize;
		}
		//---------------------------------------------------------------------------
		SizeF Direct3D9Renderer::GetViewportSize()
		{
			D3DVIEWPORT9 vp;
			if (FAILED(device->GetViewport(&vp)))
			{
				throw Misc::Exception();
			}
			
			return SizeF(vp.Width, vp.Height);
		}
		//---------------------------------------------------------------------------
		LPDIRECT3DDEVICE9 Direct3D9Renderer::GetDevice() const
		{
			return device;
		}
		//---------------------------------------------------------------------------
		bool Direct3D9Renderer::SupportsNonSquareTexture()
		{
			return supportNonSquareTex;
		}
		//---------------------------------------------------------------------------
		bool Direct3D9Renderer::SupportsNPOTTextures()
		{
			return supportNPOTTex;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		GeometryBufferPtr Direct3D9Renderer::CreateGeometryBuffer()
		{
			return std::make_shared<Direct3D9GeometryBuffer>(*this);
		}
		//---------------------------------------------------------------------------
		TextureTargetPtr Direct3D9Renderer::CreateTextureTarget()
		{
			auto textureTarget = std::make_shared<Direct3D9TextureTarget>(*this);
			textureTargets.emplace_back(textureTarget);
			return textureTarget;
		}
		//---------------------------------------------------------------------------
		TexturePtr Direct3D9Renderer::CreateTexture()
		{
			auto texture = std::shared_ptr<Direct3D9Texture>(new Direct3D9Texture(*this));
			textures.emplace_back(texture);
			return texture;
		}
		//---------------------------------------------------------------------------
		TexturePtr Direct3D9Renderer::CreateTexture(const Misc::AnsiString &filename)
		{
			auto texture = std::shared_ptr<Direct3D9Texture>(new Direct3D9Texture(*this, filename));
			textures.emplace_back(texture);
			return texture;
		}
		//---------------------------------------------------------------------------
		TexturePtr Direct3D9Renderer::CreateTexture(const SizeF &size)
		{
			auto texture = std::shared_ptr<Direct3D9Texture>(new Direct3D9Texture(*this, size));
			textures.emplace_back(texture);
			return texture;
		}
		//---------------------------------------------------------------------------
		SizeF Direct3D9Renderer::GetAdjustedSize(const SizeF &sz)
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
		float Direct3D9Renderer::GetSizeNextPOT(float _size) const
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
		void Direct3D9Renderer::BeginRendering()
		{
			stateBlock->Capture();

			RECT noScissor = { 0, 0, displaySize.Width, displaySize.Height };
			device->SetScissorRect(&noScissor);

			device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);

			device->SetVertexShader(nullptr);
			device->SetPixelShader(nullptr);

			device->SetRenderState(D3DRS_LIGHTING, FALSE);
			device->SetRenderState(D3DRS_FOGENABLE, FALSE);
			device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
			device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
			device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
			device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
			device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
			device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
			device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);

			device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
			device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

			device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);

			device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
			device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
			device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

			device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
			device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

			device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

			device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);

			device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
			device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_INVDESTALPHA);
			device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_ONE);

			device->SetTransform(D3DTS_VIEW, &Identity);
		}
		//---------------------------------------------------------------------------
		void Direct3D9Renderer::EndRendering()
		{
			stateBlock->Apply();
		}
		//---------------------------------------------------------------------------
		void Direct3D9Renderer::PreD3DReset()
		{
			stateBlock->Release();

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
		void Direct3D9Renderer::PostD3DReset()
		{
			device->CreateStateBlock(D3DSBT_ALL, &stateBlock);

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
		void Direct3D9Renderer::RemoveWeakReferences()
		{
			textureTargets.erase(std::remove_if(std::begin(textureTargets), std::end(textureTargets), [](const std::weak_ptr<Direct3D9TextureTarget> ptr) { return ptr.expired(); }), std::end(textureTargets));
			textures.erase(std::remove_if(std::begin(textures), std::end(textures), [](const std::weak_ptr<Direct3D9Texture> ptr) { return ptr.expired(); }), std::end(textures));
		}
		//---------------------------------------------------------------------------
	}
}