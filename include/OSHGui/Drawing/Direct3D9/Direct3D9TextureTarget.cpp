#include "Direct3D9TextureTarget.hpp"
#include "Direct3D9Renderer.hpp"
#include "Direct3D9Texture.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		const float Direct3D9TextureTarget::DefaultSize = 128.0f;
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D9TextureTarget::Direct3D9TextureTarget(Direct3D9Renderer &owner)
			: Direct3D9RenderTarget<TextureTarget>(owner),
			  d3d9Texture(nullptr),
			  surface(nullptr),
			  surfaceBackup(nullptr),
			  texture(std::static_pointer_cast<Direct3D9Texture>(owner.CreateTexture()))
		{
			DeclareRenderSize(SizeF(DefaultSize, DefaultSize));
		}
		//---------------------------------------------------------------------------
		Direct3D9TextureTarget::~Direct3D9TextureTarget()
		{
			CleanupRenderTexture();
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		void Direct3D9TextureTarget::DeclareRenderSize(const SizeF &size)
		{
			if (area.GetWidth() >= size.Width && area.GetHeight() >= size.Height)
			{
				return;
			}

			SetArea(RectangleF(area.GetLocation(), size));
			ResizeRenderTexture();
			Clear();
		}
		//---------------------------------------------------------------------------
		bool Direct3D9TextureTarget::IsImageryCache() const
		{
			return true;
		}
		//---------------------------------------------------------------------------
		TexturePtr Direct3D9TextureTarget::GetTexture() const
		{
			return texture;
		}
		//---------------------------------------------------------------------------
		bool Direct3D9TextureTarget::IsRenderingInverted() const
		{
			return false;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Direct3D9TextureTarget::Activate()
		{
			EnableRenderTexture();

			Direct3D9RenderTarget::Activate();
		}
		//---------------------------------------------------------------------------
		void Direct3D9TextureTarget::Deactivate()
		{
			Direct3D9RenderTarget::Deactivate();

			DisableRenderTexture();
		}
		//---------------------------------------------------------------------------
		void Direct3D9TextureTarget::Clear()
		{
			EnableRenderTexture();
			
			owner.GetDevice()->Clear(0, 0, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
			
			DisableRenderTexture();
		}
		//---------------------------------------------------------------------------
		void Direct3D9TextureTarget::InitialiseRenderTexture()
		{
			auto size = owner.GetAdjustedSize(area.GetSize());

			owner.GetDevice()->CreateTexture(size.Width, size.Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &d3d9Texture, nullptr);

			d3d9Texture->GetSurfaceLevel(0, &surface);

			texture->SetDirect3D9Texture(d3d9Texture);
			texture->SetOriginalDataSize(area.GetSize());
		}
		//---------------------------------------------------------------------------
		void Direct3D9TextureTarget::ResizeRenderTexture()
		{
			CleanupRenderTexture();
			InitialiseRenderTexture();
		}
		//---------------------------------------------------------------------------
		void Direct3D9TextureTarget::CleanupRenderTexture()
		{
			if (surface)
			{
				surface->Release();
				surface = nullptr;
			}

			if (d3d9Texture)
			{
				texture->SetDirect3D9Texture(nullptr);
				d3d9Texture->Release();
				d3d9Texture = nullptr;
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D9TextureTarget::EnableRenderTexture()
		{
			LPDIRECT3DSURFACE9 oldSurface = nullptr;
			owner.GetDevice()->GetRenderTarget(0, &oldSurface);

			if (oldSurface && oldSurface != surface)
			{
				surfaceBackup = oldSurface;
				owner.GetDevice()->SetRenderTarget(0, surface);
			}
			else if (oldSurface)
			{
				oldSurface->Release();
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D9TextureTarget::DisableRenderTexture()
		{
			owner.GetDevice()->SetRenderTarget(0, surfaceBackup);

			if (surfaceBackup)
			{
				surfaceBackup->Release();
				surfaceBackup = nullptr;
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D9TextureTarget::PreD3DReset()
		{
			if (surface)
			{
				surface->Release();
				surface = nullptr;
			}

			if (texture)
			{
				texture->PreD3DReset();
			}

			if (d3d9Texture)
			{
				d3d9Texture->Release();
				d3d9Texture = nullptr;
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D9TextureTarget::PostD3DReset()
		{
			if (!texture)
			{
				return;
			}

			texture->PostD3DReset();
			
			d3d9Texture = texture->GetDirect3D9Texture();
			if (d3d9Texture)
			{
				d3d9Texture->AddRef();
				d3d9Texture->GetSurfaceLevel(0, &surface);
			}
		}
		//---------------------------------------------------------------------------
	}
}
