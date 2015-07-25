#include "Direct3D10TextureTarget.hpp"
#include "Direct3D10Texture.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		const float Direct3D10TextureTarget::DefaultSize = 128.0f;
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D10TextureTarget::Direct3D10TextureTarget(Direct3D10Renderer &owner)
			: Direct3D10RenderTarget<TextureTarget>(owner),
			  d3dTexture(nullptr),
			  renderTargetView(nullptr),
			  renderTargetViewBackup(nullptr),
			  texture(std::static_pointer_cast<Direct3D10Texture>(owner.CreateTexture())),
			  depthStencilViewBackup(nullptr)
		{
			DeclareRenderSize(SizeF(DefaultSize, DefaultSize));
		}
		//---------------------------------------------------------------------------
		Direct3D10TextureTarget::~Direct3D10TextureTarget()
		{
			CleanupRenderTexture();
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		void Direct3D10TextureTarget::DeclareRenderSize(const SizeF &size)
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
		bool Direct3D10TextureTarget::IsImageryCache() const
		{
			return true;
		}
		//---------------------------------------------------------------------------
		TexturePtr Direct3D10TextureTarget::GetTexture() const
		{
			return texture;
		}
		//---------------------------------------------------------------------------
		bool Direct3D10TextureTarget::IsRenderingInverted() const
		{
			return false;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Direct3D10TextureTarget::Activate()
		{
			EnableRenderTexture();

			Direct3D10RenderTarget::Activate();
		}
		//---------------------------------------------------------------------------
		void Direct3D10TextureTarget::Deactivate()
		{
			Direct3D10RenderTarget::Deactivate();

			DisableRenderTexture();
		}
		//---------------------------------------------------------------------------
		void Direct3D10TextureTarget::Clear()
		{
			const float colour[] = { 0, 0, 0, 0 };

			owner.GetDevice()->ClearRenderTargetView(renderTargetView, colour);
		}
		//---------------------------------------------------------------------------
		void Direct3D10TextureTarget::InitialiseRenderTexture()
		{
			D3D10_TEXTURE2D_DESC textureDescription;
			std::memset(&textureDescription, 0, sizeof(textureDescription));
			textureDescription.Width = area.GetSize().Width;
			textureDescription.Height = area.GetSize().Height;
			textureDescription.MipLevels = 1;
			textureDescription.ArraySize = 1;
			textureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			textureDescription.SampleDesc.Count = 1;
			textureDescription.Usage = D3D10_USAGE_DEFAULT;
			textureDescription.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
			owner.GetDevice()->CreateTexture2D(&textureDescription, 0, &d3dTexture);

			D3D10_RENDER_TARGET_VIEW_DESC renderTargetDescription;
			renderTargetDescription.Format = textureDescription.Format;
			renderTargetDescription.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
			renderTargetDescription.Texture2D.MipSlice = 0;
			owner.GetDevice()->CreateRenderTargetView(d3dTexture, &renderTargetDescription, &renderTargetView);


			texture->SetDirect3D10Texture(d3dTexture);
			texture->SetOriginalDataSize(area.GetSize());
		}
		//---------------------------------------------------------------------------
		void Direct3D10TextureTarget::ResizeRenderTexture()
		{
			CleanupRenderTexture();
			InitialiseRenderTexture();
		}
		//---------------------------------------------------------------------------
		void Direct3D10TextureTarget::CleanupRenderTexture()
		{
			if (renderTargetView)
			{
				renderTargetView->Release();
				renderTargetView = nullptr;
			}

			if (d3dTexture)
			{
				texture->SetDirect3D10Texture(nullptr);
				d3dTexture->Release();
				d3dTexture = nullptr;
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D10TextureTarget::EnableRenderTexture()
		{
			owner.GetDevice()->OMGetRenderTargets(1, &renderTargetViewBackup, &depthStencilViewBackup);

			owner.GetDevice()->OMSetRenderTargets(1, &renderTargetView, nullptr);
		}
		//---------------------------------------------------------------------------
		void Direct3D10TextureTarget::DisableRenderTexture()
		{
			if (renderTargetViewBackup)
			{
				renderTargetViewBackup->Release();
			}
			if (depthStencilViewBackup)
			{
				depthStencilViewBackup->Release();
			}

			owner.GetDevice()->OMSetRenderTargets(1, &renderTargetViewBackup, depthStencilViewBackup);

			renderTargetViewBackup = nullptr;
			depthStencilViewBackup = nullptr;
		}
		//---------------------------------------------------------------------------
	}
}
