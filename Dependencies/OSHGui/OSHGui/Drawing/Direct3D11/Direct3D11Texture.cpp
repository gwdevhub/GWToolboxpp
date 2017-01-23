#include "Direct3D11Texture.hpp"
#include "../ImageLoader.hpp"
#include "../../Misc/Exceptions.hpp"
#include <d3dx11effect.h>

namespace OSHGui
{
	namespace Drawing
	{
		namespace
		{
			DXGI_FORMAT ToD3DPixelFormat(const Texture::PixelFormat format)
			{
				switch (format)
				{
				case Texture::PixelFormat::RGBA:
				case Texture::PixelFormat::RGB:
					return DXGI_FORMAT_R8G8B8A8_UNORM;
				case Texture::PixelFormat::RGBA_DXT1:
					return DXGI_FORMAT_BC1_UNORM;
				case Texture::PixelFormat::RGBA_DXT3:
					return DXGI_FORMAT_BC2_UNORM;
				case Texture::PixelFormat::RGBA_DXT5:
					return DXGI_FORMAT_BC3_UNORM;
				default:
					return DXGI_FORMAT_UNKNOWN;
				}
			}
			//---------------------------------------------------------------------------
			uint32_t CalculateDataWidth(const uint32_t width, Texture::PixelFormat format)
			{
				switch (format)
				{
					case Texture::PixelFormat::RGBA:
					case Texture::PixelFormat::RGB:
						return width * 4;
					case Texture::PixelFormat::RGBA_DXT1:
						return ((width + 3) / 4) * 8;
					case Texture::PixelFormat::RGBA_DXT3:
					case Texture::PixelFormat::RGBA_DXT5:
						return ((width + 3) / 4) * 16;
					default:
						return 0;
				}
			}
			//---------------------------------------------------------------------------
			void BlitRGBAToD3DCOLORSurface(const uint32_t *src, uint32_t *dst, const SizeF &size, uint32_t pitch)
			{
				for (auto i = 0; i < size.Height; ++i)
				{
					for (auto j = 0; j < size.Width; ++j)
					{
						auto pixel = src[j];
						auto tmp = pixel & 0x00FF00FF;
						dst[j] = pixel & 0xFF00FF00 | (tmp << 16) | (tmp >> 16);
					}

					dst += pitch / sizeof(uint32_t);
					src += static_cast<uint32_t>(size.Width);
				}
			}
		}
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D11Texture::Direct3D11Texture(Direct3D11Renderer::IDevice11 &_device)
			: device(_device),
			  texture(nullptr),
			  size(0, 0),
			  dataSize(0, 0),
			  texelScaling(0, 0),
			  resourceView(nullptr)
		{

		}
		//---------------------------------------------------------------------------
		Direct3D11Texture::Direct3D11Texture(Direct3D11Renderer::IDevice11 &_device, const Misc::AnsiString &path)
			: device(_device),
			  texture(nullptr),
			  texelScaling(0, 0),
			  resourceView(nullptr)
		{
			LoadFromFile(path);
		}
		//---------------------------------------------------------------------------
		Direct3D11Texture::Direct3D11Texture(Direct3D11Renderer::IDevice11 &_device, const SizeF &_size)
			: device(_device),
			  texture(nullptr),
			  size(0, 0),
			  dataSize(size),
			  texelScaling(0, 0),
			  resourceView(nullptr)
		{
			CreateDirect3D11Texture(_size, DXGI_FORMAT_R8G8B8A8_UNORM, nullptr);
		}
		//---------------------------------------------------------------------------
		Direct3D11Texture::~Direct3D11Texture()
		{
			CleanupDirect3D11Texture();
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		void Direct3D11Texture::SetDirect3D11Texture(ID3D11Texture2D *_texture)
		{
			if (texture != _texture)
			{
				CleanupDirect3D11Texture();
				dataSize.Width = dataSize.Height = 0;

				texture = _texture;
				if (texture)
				{
					texture->AddRef();
				}
			}

			InitialiseShaderResourceView();

			UpdateTextureSize();
			dataSize = size;
			UpdateCachedScaleValues();
		}
		//---------------------------------------------------------------------------
		ID3D11Texture2D* Direct3D11Texture::GetDirect3D11Texture() const
		{
			return texture;
		}
		//---------------------------------------------------------------------------
		ID3D11ShaderResourceView* Direct3D11Texture::GetDirect3DShaderResourceView() const
		{
			return resourceView;
		}
		//---------------------------------------------------------------------------
		const SizeF& Direct3D11Texture::GetSize() const
		{
			return size;
		}
		//---------------------------------------------------------------------------
		void Direct3D11Texture::SetOriginalDataSize(const SizeF& size)
		{
			dataSize = size;
			UpdateCachedScaleValues();
		}
		//---------------------------------------------------------------------------
		const SizeF& Direct3D11Texture::GetOriginalDataSize() const
		{
			return dataSize;
		}
		//---------------------------------------------------------------------------
		const std::pair<float, float>& Direct3D11Texture::GetTexelScaling() const
		{
			return texelScaling;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Direct3D11Texture::LoadFromFile(const Misc::AnsiString &filename)
		{
			auto imageData = LoadImageFromFileToRGBABuffer(filename);
			LoadFromMemory(imageData.Data.data(), imageData.Size, PixelFormat::RGBA);
		}
		//---------------------------------------------------------------------------
		void Direct3D11Texture::LoadFromMemory(const void *buffer, const SizeF &size, PixelFormat format)
		{
			if (!IsPixelFormatSupported(format))
			{
				throw Misc::NotSupportedException();
			}

			D3D11_SUBRESOURCE_DATA subresourceData;
			std::memset(&subresourceData, 0, sizeof(subresourceData));
			subresourceData.SysMemPitch = CalculateDataWidth(size.Width, format);

			if (format == PixelFormat::RGB)
			{
				auto src = static_cast<const char*>(buffer);
				std::vector<uint8_t> rgba(size.Width * size.Height * 4);
				
				for (int i = 0; i < size.Width * size.Height; ++i)
				{
					rgba[i * 4 + 0] = src[i * 3 + 0];
					rgba[i * 4 + 1] = src[i * 3 + 1];
					rgba[i * 4 + 2] = src[i * 3 + 2];
					rgba[i * 4 + 3] = 0xFF;
				}

				subresourceData.pSysMem = rgba.data();
				CreateDirect3D11Texture(size, ToD3DPixelFormat(format), &subresourceData);
			}
			else
			{
				subresourceData.pSysMem = buffer;
				CreateDirect3D11Texture(size, ToD3DPixelFormat(format), &subresourceData);
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D11Texture::CreateDirect3D11Texture(const SizeF &size, DXGI_FORMAT format, D3D11_SUBRESOURCE_DATA *subresourceData)
		{
			CleanupDirect3D11Texture();

			D3D11_TEXTURE2D_DESC textureDescription;
			std::memset(&textureDescription, 0, sizeof(textureDescription));
			textureDescription.Width = size.Width;
			textureDescription.Height = size.Height;
			textureDescription.ArraySize = 1;
			textureDescription.SampleDesc.Count = 1;
			textureDescription.SampleDesc.Quality = 0;
			textureDescription.Format = format;
			textureDescription.Usage = D3D11_USAGE_DEFAULT;
			textureDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			textureDescription.CPUAccessFlags = 0;
			textureDescription.MiscFlags = 0;
			textureDescription.MipLevels = 1;

			if (FAILED(device.Device->CreateTexture2D(&textureDescription, subresourceData, &texture)))
			{
				throw Misc::Exception();
			}

			InitialiseShaderResourceView();

			dataSize = size;
			UpdateTextureSize();
			UpdateCachedScaleValues();
		}
		//---------------------------------------------------------------------------
		void Direct3D11Texture::CleanupDirect3D11Texture()
		{
			if (resourceView)
			{
				resourceView->Release();
				resourceView = nullptr;
			}

			if (texture)
			{
				texture->Release();
				texture = nullptr;
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D11Texture::UpdateCachedScaleValues()
		{
			const float orgW = dataSize.Width;
			const float texW = size.Width;

			texelScaling.first = 1.0f / (orgW == texW ? orgW : texW);

			const float orgH = dataSize.Height;
			const float texH = size.Height;

			texelScaling.second = 1.0f / (orgH == texH ? orgH : texH);
		}
		//---------------------------------------------------------------------------
		void Direct3D11Texture::UpdateTextureSize()
		{
			if (texture)
			{
				D3D11_TEXTURE2D_DESC surfaceDescription;
				texture->GetDesc(&surfaceDescription);
				size.Width = surfaceDescription.Width;
				size.Height = surfaceDescription.Height;
			}
			else
			{
				size.Height = size.Width = 0.0f;
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D11Texture::InitialiseShaderResourceView()
		{
			if (!texture)
			{
				return;
			}

			D3D11_TEXTURE2D_DESC textureDescription;
			texture->GetDesc(&textureDescription);

			ID3D11Resource *resource = 0;
			texture->QueryInterface(__uuidof(ID3D11Resource), reinterpret_cast<LPVOID*>(&resource));

			D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDescription;
			shaderResourceViewDescription.Format = textureDescription.Format;
			shaderResourceViewDescription.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
			shaderResourceViewDescription.Texture2D.MostDetailedMip = 0;
			shaderResourceViewDescription.Texture2D.MipLevels = textureDescription.MipLevels;
			device.Device->CreateShaderResourceView(resource, &shaderResourceViewDescription, &resourceView);
			
			resource->Release();
		}
		//---------------------------------------------------------------------------
		bool Direct3D11Texture::IsPixelFormatSupported(const PixelFormat format) const
		{
			switch (format)
			{
				case PixelFormat::RGBA:
				case PixelFormat::RGB:
				case PixelFormat::RGBA_DXT1:
				case PixelFormat::RGBA_DXT3:
				case PixelFormat::RGBA_DXT5:
					return true;
				default:
					return false;
			}
		}
		//---------------------------------------------------------------------------
	}
}
