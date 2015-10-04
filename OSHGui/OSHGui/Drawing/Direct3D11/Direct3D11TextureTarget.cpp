#include "Direct3D11TextureTarget.hpp"
#include "Direct3D11Texture.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		const float Direct3D11TextureTarget::DefaultSize = 128.0f;
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D11TextureTarget::Direct3D11TextureTarget(Direct3D11Renderer &owner)
			: Direct3D11RenderTarget<TextureTarget>(owner),
			  d3dTexture(nullptr),
			  renderTargetView(nullptr),
			  renderTargetViewBackup(nullptr),
			  texture(std::static_pointer_cast<Direct3D11Texture>(owner.CreateTexture())),
			  depthStencilViewBackup(nullptr)
		{
			DeclareRenderSize(SizeF(DefaultSize, DefaultSize));
		}
		//---------------------------------------------------------------------------
		Direct3D11TextureTarget::~Direct3D11TextureTarget()
		{
			CleanupRenderTexture();
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		void Direct3D11TextureTarget::DeclareRenderSize(const SizeF &size)
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
		bool Direct3D11TextureTarget::IsImageryCache() const
		{
			return true;
		}
		//---------------------------------------------------------------------------
		TexturePtr Direct3D11TextureTarget::GetTexture() const
		{
			return texture;
		}
		//---------------------------------------------------------------------------
		bool Direct3D11TextureTarget::IsRenderingInverted() const
		{
			return false;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Direct3D11TextureTarget::Activate()
		{
			EnableRenderTexture();

			Direct3D11RenderTarget::Activate();
		}
		//---------------------------------------------------------------------------
		void Direct3D11TextureTarget::Deactivate()
		{
			Direct3D11RenderTarget::Deactivate();

			DisableRenderTexture();
		}
		//---------------------------------------------------------------------------
		void Direct3D11TextureTarget::Clear()
		{
			const float colour[] = { 0, 0, 0, 0 };

			owner.GetDevice().Context->ClearRenderTargetView(renderTargetView, colour);
		}
		//---------------------------------------------------------------------------
		void Direct3D11TextureTarget::InitialiseRenderTexture()
		{
			D3D11_TEXTURE2D_DESC textureDescription;
			std::memset(&textureDescription, 0, sizeof(textureDescription));
			textureDescription.Width = area.GetSize().Width;
			textureDescription.Height = area.GetSize().Height;
			textureDescription.MipLevels = 1;
			textureDescription.ArraySize = 1;
			textureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			textureDescription.SampleDesc.Count = 1;
			textureDescription.Usage = D3D11_USAGE_DEFAULT;
			textureDescription.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			owner.GetDevice().Device->CreateTexture2D(&textureDescription, 0, &d3dTexture);

			D3D11_RENDER_TARGET_VIEW_DESC renderTargetDescription;
			renderTargetDescription.Format = textureDescription.Format;
			renderTargetDescription.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			renderTargetDescription.Texture2D.MipSlice = 0;
			owner.GetDevice().Device->CreateRenderTargetView(d3dTexture, &renderTargetDescription, &renderTargetView);


			texture->SetDirect3D11Texture(d3dTexture);
			texture->SetOriginalDataSize(area.GetSize());
		}
		//---------------------------------------------------------------------------
		void Direct3D11TextureTarget::ResizeRenderTexture()
		{
			CleanupRenderTexture();
			InitialiseRenderTexture();
		}
		//---------------------------------------------------------------------------
		void Direct3D11TextureTarget::CleanupRenderTexture()
		{
			if (renderTargetView)
			{
				renderTargetView->Release();
				renderTargetView = nullptr;
			}

			if (d3dTexture)
			{
				texture->SetDirect3D11Texture(nullptr);
				d3dTexture->Release();
				d3dTexture = nullptr;
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D11TextureTarget::EnableRenderTexture()
		{
			owner.GetDevice().Context->OMGetRenderTargets(1, &renderTargetViewBackup, &depthStencilViewBackup);

			owner.GetDevice().Context->OMSetRenderTargets(1, &renderTargetView, nullptr);
		}
		//---------------------------------------------------------------------------
		void Direct3D11TextureTarget::DisableRenderTexture()
		{
			if (renderTargetViewBackup)
			{
				renderTargetViewBackup->Release();
			}
			if (depthStencilViewBackup)
			{
				depthStencilViewBackup->Release();
			}

			owner.GetDevice().Context->OMSetRenderTargets(1, &renderTargetViewBackup, depthStencilViewBackup);

			renderTargetViewBackup = nullptr;
			depthStencilViewBackup = nullptr;
		}
		//---------------------------------------------------------------------------
	}
}
