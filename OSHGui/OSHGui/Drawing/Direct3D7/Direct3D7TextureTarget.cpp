#include "Direct3D7TextureTarget.hpp"
#include "Direct3D7Renderer.hpp"
#include "Direct3D7Texture.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		const float Direct3D7TextureTarget::DefaultSize = 128.0f;
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D7TextureTarget::Direct3D7TextureTarget(Direct3D7Renderer &owner)
			: Direct3D7RenderTarget<TextureTarget>(owner),
			  surface(nullptr),
			  surfaceBackup(nullptr),
			  texture(std::static_pointer_cast<Direct3D7Texture>(owner.CreateTexture()))
		{
			DeclareRenderSize(SizeF(DefaultSize, DefaultSize));
		}
		//---------------------------------------------------------------------------
		Direct3D7TextureTarget::~Direct3D7TextureTarget()
		{
			CleanupRenderTexture();
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		void Direct3D7TextureTarget::DeclareRenderSize(const SizeF &size)
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
		bool Direct3D7TextureTarget::IsImageryCache() const
		{
			return true;
		}
		//---------------------------------------------------------------------------
		TexturePtr Direct3D7TextureTarget::GetTexture() const
		{
			return texture;
		}
		//---------------------------------------------------------------------------
		bool Direct3D7TextureTarget::IsRenderingInverted() const
		{
			return false;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Direct3D7TextureTarget::Activate()
		{
			EnableRenderTexture();

			Direct3D7RenderTarget::Activate();
		}
		//---------------------------------------------------------------------------
		void Direct3D7TextureTarget::Deactivate()
		{
			Direct3D7RenderTarget::Deactivate();

			DisableRenderTexture();
		}
		//---------------------------------------------------------------------------
		void Direct3D7TextureTarget::Clear()
		{
			EnableRenderTexture();
			
			owner.GetDevice().Device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
			
			DisableRenderTexture();
		}
		//---------------------------------------------------------------------------
		void Direct3D7TextureTarget::InitialiseRenderTexture()
		{
			auto size = owner.GetAdjustedSize(area.GetSize());

			surface = owner.CreateSurface(size, true);

			texture->SetDirect3D7Texture(surface);
			texture->SetOriginalDataSize(area.GetSize());
		}
		//---------------------------------------------------------------------------
		void Direct3D7TextureTarget::ResizeRenderTexture()
		{
			CleanupRenderTexture();
			InitialiseRenderTexture();
		}
		//---------------------------------------------------------------------------
		void Direct3D7TextureTarget::CleanupRenderTexture()
		{
			if (surface)
			{
				texture->SetDirect3D7Texture(nullptr);
				surface->Release();
				surface = nullptr;
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D7TextureTarget::EnableRenderTexture()
		{
			LPDIRECTDRAWSURFACE7 oldSurface = nullptr;
			owner.GetDevice().Device->GetRenderTarget(&oldSurface);

			if (oldSurface && oldSurface != surface)
			{
				surfaceBackup = oldSurface;
				owner.GetDevice().Device->SetRenderTarget(surface, 0);
			}
			else if (oldSurface)
			{
				oldSurface->Release();
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D7TextureTarget::DisableRenderTexture()
		{
			owner.GetDevice().Device->SetRenderTarget(surfaceBackup, 0);

			if (surfaceBackup)
			{
				surfaceBackup->Release();
				surfaceBackup = nullptr;
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D7TextureTarget::PreD3DReset()
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
		}
		//---------------------------------------------------------------------------
		void Direct3D7TextureTarget::PostD3DReset()
		{
			if (!texture)
			{
				return;
			}

			texture->PostD3DReset();
			
			surface = texture->GetDirect3D7Texture();
		}
		//---------------------------------------------------------------------------
	}
}
