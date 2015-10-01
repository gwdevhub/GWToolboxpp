#include "Direct3D8TextureTarget.hpp"
#include "Direct3D8Renderer.hpp"
#include "Direct3D8Texture.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		const float Direct3D8TextureTarget::DefaultSize = 128.0f;
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D8TextureTarget::Direct3D8TextureTarget(Direct3D8Renderer &owner)
			: Direct3D8RenderTarget<TextureTarget>(owner),
			  d3d8Texture(nullptr),
			  surface(nullptr),
			  surfaceBackup(nullptr),
			  texture(std::static_pointer_cast<Direct3D8Texture>(owner.CreateTexture()))
		{
			DeclareRenderSize(SizeF(DefaultSize, DefaultSize));
		}
		//---------------------------------------------------------------------------
		Direct3D8TextureTarget::~Direct3D8TextureTarget()
		{
			CleanupRenderTexture();
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		void Direct3D8TextureTarget::DeclareRenderSize(const SizeF &size)
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
		bool Direct3D8TextureTarget::IsImageryCache() const
		{
			return true;
		}
		//---------------------------------------------------------------------------
		TexturePtr Direct3D8TextureTarget::GetTexture() const
		{
			return texture;
		}
		//---------------------------------------------------------------------------
		bool Direct3D8TextureTarget::IsRenderingInverted() const
		{
			return false;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Direct3D8TextureTarget::Activate()
		{
			EnableRenderTexture();

			Direct3D8RenderTarget::Activate();
		}
		//---------------------------------------------------------------------------
		void Direct3D8TextureTarget::Deactivate()
		{
			Direct3D8RenderTarget::Deactivate();

			DisableRenderTexture();
		}
		//---------------------------------------------------------------------------
		void Direct3D8TextureTarget::Clear()
		{
			EnableRenderTexture();
			
			owner.GetDevice()->Clear(0, 0, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
			
			DisableRenderTexture();
		}
		//---------------------------------------------------------------------------
		void Direct3D8TextureTarget::InitialiseRenderTexture()
		{
			auto size = owner.GetAdjustedSize(area.GetSize());

			owner.GetDevice()->CreateTexture(size.Width, size.Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &d3d8Texture);

			d3d8Texture->GetSurfaceLevel(0, &surface);

			texture->SetDirect3D8Texture(d3d8Texture);
			texture->SetOriginalDataSize(area.GetSize());
		}
		//---------------------------------------------------------------------------
		void Direct3D8TextureTarget::ResizeRenderTexture()
		{
			CleanupRenderTexture();
			InitialiseRenderTexture();
		}
		//---------------------------------------------------------------------------
		void Direct3D8TextureTarget::CleanupRenderTexture()
		{
			if (surface)
			{
				surface->Release();
				surface = nullptr;
			}

			if (d3d8Texture)
			{
				texture->SetDirect3D8Texture(nullptr);
				d3d8Texture->Release();
				d3d8Texture = nullptr;
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D8TextureTarget::EnableRenderTexture()
		{
			LPDIRECT3DSURFACE8 oldSurface = nullptr;
			owner.GetDevice()->GetRenderTarget(&oldSurface);

			if (oldSurface && oldSurface != surface)
			{
				surfaceBackup = oldSurface;
				owner.GetDevice()->SetRenderTarget(surface, nullptr);
			}
			else if (oldSurface)
			{
				oldSurface->Release();
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D8TextureTarget::DisableRenderTexture()
		{
			owner.GetDevice()->SetRenderTarget(surfaceBackup, nullptr);

			if (surfaceBackup)
			{
				surfaceBackup->Release();
				surfaceBackup = nullptr;
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D8TextureTarget::PreD3DReset()
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

			if (d3d8Texture)
			{
				d3d8Texture->Release();
				d3d8Texture = nullptr;
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D8TextureTarget::PostD3DReset()
		{
			if (!texture)
			{
				return;
			}

			texture->PostD3DReset();
			
			d3d8Texture = texture->GetDirect3D8Texture();
			if (d3d8Texture)
			{
				d3d8Texture->AddRef();
				d3d8Texture->GetSurfaceLevel(0, &surface);
			}
		}
		//---------------------------------------------------------------------------
	}
}
