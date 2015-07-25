#include "Direct3D7Texture.hpp"
#include "Direct3D7X.hpp"
#include "../ImageLoader.hpp"
#include "../../Misc/Exceptions.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D7Texture::Direct3D7Texture(Direct3D7Renderer &_owner)
			: owner(_owner),
			  texture(nullptr),
			  size(0.0f, 0.0f),
			  dataSize(0.0f, 0.0f),
			  texelScaling(0.0f, 0.0f)
		{

		}
		//---------------------------------------------------------------------------
		Direct3D7Texture::Direct3D7Texture(Direct3D7Renderer &_owner, const Misc::AnsiString &filename)
			: owner(_owner),
			  texture(nullptr),
			  texelScaling(0.0f, 0.0f)
		{
			LoadFromFile(filename);
		}
		//---------------------------------------------------------------------------
		Direct3D7Texture::Direct3D7Texture(Direct3D7Renderer &_owner, const SizeF &_size)
			: owner(_owner),
			  texture(nullptr),
			  size(0.0f, 0.0f),
			  dataSize(size),
			  texelScaling(0.0f, 0.0f)
		{
			CreateDirect3D7Texture(_size);
		}
		//---------------------------------------------------------------------------
		Direct3D7Texture::~Direct3D7Texture()
		{
			CleanupDirect3D7Texture();
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		void Direct3D7Texture::SetDirect3D7Texture(LPDIRECTDRAWSURFACE7 _texture)
		{
			if (texture != _texture)
			{
				CleanupDirect3D7Texture();
				dataSize.Width = dataSize.Height = 0;

				texture = _texture;
				if (texture)
				{
					texture->AddRef();
				}
			}

			UpdateTextureSize();
			dataSize = size;
			UpdateCachedScaleValues();
		}
		//---------------------------------------------------------------------------
		LPDIRECTDRAWSURFACE7 Direct3D7Texture::GetDirect3D7Texture() const
		{
			return texture;
		}
		//---------------------------------------------------------------------------
		const SizeF& Direct3D7Texture::GetSize() const
		{
			return size;
		}
		//---------------------------------------------------------------------------
		const SizeF& Direct3D7Texture::GetOriginalDataSize() const
		{
			return dataSize;
		}
		//---------------------------------------------------------------------------
		const std::pair<float, float>& Direct3D7Texture::GetTexelScaling() const
		{
			return texelScaling;
		}
		//---------------------------------------------------------------------------
		void Direct3D7Texture::SetOriginalDataSize(const SizeF& size)
		{
			dataSize = size;
			UpdateCachedScaleValues();
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Direct3D7Texture::LoadFromFile(const Misc::AnsiString &filename)
		{
			auto imageData = LoadImageFromFileToRGBABuffer(filename);
			LoadFromMemory(imageData.Data.data(), imageData.Size, PixelFormat::RGBA);
		}
		//---------------------------------------------------------------------------
		void Direct3D7Texture::LoadFromMemory(const void *buffer, const SizeF &size, PixelFormat pixelFormat)
		{
			if (!IsPixelFormatSupported(pixelFormat))
			{
				throw Misc::NotSupportedException();
			}

			CreateDirect3D7Texture(size);

			DDSURFACEDESC2 description;
			description.dwSize = sizeof(description);
			texture->GetSurfaceDesc(&description);

			if (FAILED(texture->Lock(nullptr, &description, 0, 0)))
			{
				throw Misc::Exception();
			}

			auto CopyPixels = [](const uint32_t *src, uint32_t *dst, const SizeF &size)
			{
				for (auto i = 0; i < size.Width * size.Height; ++i)
				{
					auto data = *src++;

					auto r = ((data >> 0) & 0xFF);
					auto g = ((data >> 8) & 0xFF);
					auto b = ((data >> 16) & 0xFF);
					auto a = ((data >> 24) & 0xFF);

					*dst++ = (a << 24)|(r << 16)|(g << 8)|b;
				}
			};

			CopyPixels(static_cast<const uint32_t*>(buffer), static_cast<uint32_t*>(description.lpSurface), size);

			texture->Unlock(0);
		}
		//---------------------------------------------------------------------------
		void Direct3D7Texture::CreateDirect3D7Texture(const SizeF &size)
		{
			CleanupDirect3D7Texture();

			auto textureSize = owner.GetAdjustedSize(size);

			texture = owner.CreateSurface(textureSize, false);

			dataSize = size;
			UpdateTextureSize();
			UpdateCachedScaleValues();
		}
		//---------------------------------------------------------------------------
		void Direct3D7Texture::CleanupDirect3D7Texture()
		{
			if (texture)
			{
				texture->Release();
				texture = nullptr;
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D7Texture::UpdateCachedScaleValues()
		{
			const float orgW = dataSize.Width;
			const float texW = size.Width;

			texelScaling.first = 1.0f / ((orgW == texW) ? orgW : texW);

			const float orgH = dataSize.Height;
			const float texH = size.Height;

			texelScaling.second = 1.0f / ((orgH == texH) ? orgH : texH);
		}
		//---------------------------------------------------------------------------
		void Direct3D7Texture::UpdateTextureSize()
		{
			DDSURFACEDESC2 description;
			if (texture && SUCCEEDED(texture->GetSurfaceDesc(&description)))
			{
				size.Width  = description.dwWidth;
				size.Height = description.dwHeight;
			}
			else
			{
				size = dataSize;
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D7Texture::PreD3DReset()
		{
			if (!texture)
			{
				return;
			}

			DDSURFACEDESC2 surfaceDescription;
			texture->GetSurfaceDesc(&surfaceDescription);

			if (surfaceDescription.ddsCaps.dwCaps2 == DDSCAPS2_D3DTEXTUREMANAGE)
			{
				return;
			}

			texture->Release();
			texture = nullptr;
		}
		//---------------------------------------------------------------------------
		void Direct3D7Texture::PostD3DReset()
		{
			auto textureSize = owner.GetAdjustedSize(size);

			texture = owner.CreateSurface(textureSize, false);
		}
		//---------------------------------------------------------------------------
		bool Direct3D7Texture::IsPixelFormatSupported(const PixelFormat format) const
		{
			return format == Texture::PixelFormat::RGBA;
		}
		//---------------------------------------------------------------------------
	}
}
