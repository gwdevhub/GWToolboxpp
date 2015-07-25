#include "Direct3D7Renderer.hpp"
#include "Direct3D7Texture.hpp"
#include "Direct3D7GeometryBuffer.hpp"
#include "Direct3D7RenderTarget.hpp"
#include "Direct3D7ViewportTarget.hpp"
#include "Direct3D7TextureTarget.hpp"
#include "../../Misc/Exceptions.hpp"

#include <algorithm>

namespace OSHGui
{
	namespace Drawing
	{
		static const D3DMATRIX Identity
		(
			1.0, 0.0, 0.0, 0.0,
			0.0, 1.0, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			0.0, 0.0, 0.0, 1.0
		);
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D7Renderer::Direct3D7Renderer(LPDIRECT3DDEVICE7 _device)
			: device(_device),
			  displaySize(GetViewportSize()),
			  displayDPI(96, 96),
			  stateBlockHandle(0)
		{
			D3DDEVICEDESC7 caps;
			device.Device->GetCaps(&caps);

			maxTextureSize = std::min(caps.dwMaxTextureHeight, caps.dwMaxTextureWidth);

			supportNonSquareTex = !(caps.dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_SQUAREONLY);

			supportNPOTTex = !(caps.dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_POW2) || (caps.dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL);

			defaultTarget = std::make_shared<Direct3D7ViewportTarget>(*this);

			PostD3DReset(); //indirect initialize
		}
		//---------------------------------------------------------------------------
		Direct3D7Renderer::~Direct3D7Renderer()
		{
			if (stateBlockHandle)
			{
				device.Device->DeleteStateBlock(stateBlockHandle);
			}
			if (device.DirectDraw)
			{
				device.DirectDraw->Release();
			}
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		RenderTargetPtr& Direct3D7Renderer::GetDefaultRenderTarget()
		{
			return defaultTarget;
		}
		//---------------------------------------------------------------------------
		void Direct3D7Renderer::SetDisplaySize(const SizeF &size)
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
		const SizeF& Direct3D7Renderer::GetDisplaySize() const
		{
			return displaySize;
		}
		//---------------------------------------------------------------------------
		const PointF& Direct3D7Renderer::GetDisplayDPI() const
		{
			return displayDPI;
		}
		//---------------------------------------------------------------------------
		uint32_t Direct3D7Renderer::GetMaximumTextureSize() const
		{
			return maxTextureSize;
		}
		//---------------------------------------------------------------------------
		SizeF Direct3D7Renderer::GetViewportSize()
		{
			D3DVIEWPORT7 vp;
			if (FAILED(device.Device->GetViewport(&vp)))
			{
				throw Misc::Exception();
			}

			return SizeF(vp.dwWidth, vp.dwHeight);
		}
		//---------------------------------------------------------------------------
		Direct3D7Renderer::IDevice7& Direct3D7Renderer::GetDevice()
		{
			return device;
		}
		//---------------------------------------------------------------------------
		bool Direct3D7Renderer::SupportsNonSquareTexture()
		{
			return supportNonSquareTex;
		}
		//---------------------------------------------------------------------------
		bool Direct3D7Renderer::SupportsNPOTTextures()
		{
			return supportNPOTTex;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		GeometryBufferPtr Direct3D7Renderer::CreateGeometryBuffer()
		{
			return std::make_shared<Direct3D7GeometryBuffer>(*this);
		}
		//---------------------------------------------------------------------------
		TextureTargetPtr Direct3D7Renderer::CreateTextureTarget()
		{
			auto textureTarget = std::make_shared<Direct3D7TextureTarget>(*this);
			textureTargets.emplace_back(textureTarget);
			return textureTarget;
		}
		//---------------------------------------------------------------------------
		TexturePtr Direct3D7Renderer::CreateTexture()
		{
			auto texture = std::shared_ptr<Direct3D7Texture>(new Direct3D7Texture(*this));
			textures.emplace_back(texture);
			return texture;
		}
		//---------------------------------------------------------------------------
		TexturePtr Direct3D7Renderer::CreateTexture(const Misc::AnsiString &filename)
		{
			auto texture = std::shared_ptr<Direct3D7Texture>(new Direct3D7Texture(*this, filename));
			textures.emplace_back(texture);
			return texture;
		}
		//---------------------------------------------------------------------------
		TexturePtr Direct3D7Renderer::CreateTexture(const SizeF &size)
		{
			auto texture = std::shared_ptr<Direct3D7Texture>(new Direct3D7Texture(*this, size));
			textures.emplace_back(texture);
			return texture;
		}
		//---------------------------------------------------------------------------
		void Direct3D7Renderer::BeginRendering()
		{
			device.Device->CaptureStateBlock(stateBlockHandle);

			device.Device->SetRenderState(D3DRENDERSTATE_LIGHTING, FALSE);
			device.Device->SetRenderState(D3DRENDERSTATE_FOGENABLE, FALSE);
			device.Device->SetRenderState(D3DRENDERSTATE_ZENABLE, FALSE);
			device.Device->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, FALSE);
			device.Device->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
			device.Device->SetRenderState(D3DRENDERSTATE_FILLMODE, D3DFILL_SOLID);
			device.Device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE);

			device.Device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			device.Device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			device.Device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);

			device.Device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
			device.Device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
			device.Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

			device.Device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

			device.Device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);

			device.Device->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
			device.Device->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);

			device.Device->SetTransform(D3DTRANSFORMSTATE_VIEW, (D3DMATRIX*)&Identity);
		}
		//---------------------------------------------------------------------------
		void Direct3D7Renderer::EndRendering()
		{
			device.Device->ApplyStateBlock(stateBlockHandle);
		}
		//---------------------------------------------------------------------------
		void Direct3D7Renderer::PreD3DReset()
		{
			device.Device->DeleteStateBlock(stateBlockHandle);

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
		void Direct3D7Renderer::PostD3DReset()
		{
			device.Device->CreateStateBlock(D3DSBT_ALL, &stateBlockHandle);

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
		SizeF Direct3D7Renderer::GetAdjustedSize(const SizeF &sz)
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
		float Direct3D7Renderer::GetSizeNextPOT(float _size) const
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
		LPDIRECTDRAWSURFACE7 Direct3D7Renderer::CreateSurface(const SizeF &size, bool isRenderTarget) const
		{
			DDSURFACEDESC2 surfaceDescription;
			std::memset(&surfaceDescription, 0, sizeof(DDSURFACEDESC2));
			surfaceDescription.dwSize = sizeof(DDSURFACEDESC2);
			surfaceDescription.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
			surfaceDescription.ddsCaps.dwCaps = DDSCAPS_TEXTURE;
			surfaceDescription.ddsCaps.dwCaps2 = 0;
			surfaceDescription.dwWidth = size.Width;
			surfaceDescription.dwHeight = size.Height;

			if (isRenderTarget)
			{
				surfaceDescription.ddsCaps.dwCaps |= DDSCAPS_3DDEVICE | DDSCAPS_LOCALVIDMEM | DDSCAPS_VIDEOMEMORY;
				surfaceDescription.ddsCaps.dwCaps2 = 0;
			}
			else
			{
				surfaceDescription.ddsCaps.dwCaps2 = DDSCAPS2_D3DTEXTUREMANAGE;
			}

			surfaceDescription.ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
			surfaceDescription.ddpfPixelFormat.dwRGBBitCount = 32;
			surfaceDescription.ddpfPixelFormat.dwRGBAlphaBitMask = 0xFF000000;
			surfaceDescription.ddpfPixelFormat.dwRBitMask = 0x00FF0000;
			surfaceDescription.ddpfPixelFormat.dwGBitMask = 0x0000FF00;
			surfaceDescription.ddpfPixelFormat.dwBBitMask = 0x000000FF;

			if (isRenderTarget)
			{
				LPDIRECTDRAWSURFACE7 gdiSurface;
				device.DirectDraw->GetGDISurface(&gdiSurface);
				gdiSurface->GetPixelFormat(&surfaceDescription.ddpfPixelFormat);
				gdiSurface->Release();
			}

			LPDIRECTDRAWSURFACE7 surface = nullptr;
			if (FAILED(device.DirectDraw->CreateSurface(&surfaceDescription, &surface, nullptr)))
			{
				throw Misc::Exception();
			}

			if (isRenderTarget)
			{
				DDSCAPS2 bufferCaps;
				std::memset(&bufferCaps, 0, sizeof(DDSURFACEDESC2));
				bufferCaps.dwCaps = DDSCAPS_ZBUFFER;
				bufferCaps.dwCaps2 = bufferCaps.dwCaps3 = bufferCaps.dwCaps4 = 0;

				DDSURFACEDESC2 bufferSurfaceDescription;
				std::memset(&bufferSurfaceDescription, 0, sizeof(DDSURFACEDESC2));
				bufferSurfaceDescription.dwSize = sizeof(DDSURFACEDESC2);

				LPDIRECTDRAWSURFACE7 backBuffer;
				if (SUCCEEDED(device.Device->GetRenderTarget(&backBuffer)))
				{
					LPDIRECTDRAWSURFACE7 zBuffer;
					if (SUCCEEDED(backBuffer->GetAttachedSurface(&bufferCaps, &zBuffer)))
					{
						zBuffer->GetSurfaceDesc(&bufferSurfaceDescription);

						bufferSurfaceDescription.dwWidth = surfaceDescription.dwWidth;
						bufferSurfaceDescription.dwHeight = surfaceDescription.dwHeight;

						LPDIRECTDRAWSURFACE7 attachSurface;
						device.DirectDraw->CreateSurface(&bufferSurfaceDescription, &attachSurface, nullptr);
						surface->AddAttachedSurface(attachSurface);

						backBuffer->Release();
						zBuffer->Release();
						attachSurface->Release();
					}
				}
			}

			return surface;
		}
		//---------------------------------------------------------------------------
		void Direct3D7Renderer::RemoveWeakReferences()
		{
			textureTargets.erase(std::remove_if(std::begin(textureTargets), std::end(textureTargets), [](const std::weak_ptr<Direct3D7TextureTarget> ptr) { return ptr.expired(); }), std::end(textureTargets));
			textures.erase(std::remove_if(std::begin(textures), std::end(textures), [](const std::weak_ptr<Direct3D7Texture> ptr) { return ptr.expired(); }), std::end(textures));
		}
		//---------------------------------------------------------------------------
	}
}