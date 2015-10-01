#include "Direct3D9Texture.hpp"
#include "../ImageLoader.hpp"
#include "../../Misc/Exceptions.hpp"
#include <d3dx9.h>

namespace OSHGui
{
	namespace Drawing
	{
		namespace
		{
			static D3DFORMAT ToD3DPixelFormat(const Texture::PixelFormat format)
			{
				switch (format)
				{
				case Texture::PixelFormat::RGBA:
					return D3DFMT_A8R8G8B8;
				case Texture::PixelFormat::RGB:
					return D3DFMT_X8R8G8B8;
				case Texture::PixelFormat::RGB_565:
					return D3DFMT_R5G6B5;
				case Texture::PixelFormat::RGBA_4444:
					return D3DFMT_A4R4G4B4;
				case Texture::PixelFormat::RGBA_DXT1:
					return D3DFMT_DXT1;
				case Texture::PixelFormat::RGBA_DXT3:
					return D3DFMT_DXT3;
				case Texture::PixelFormat::RGBA_DXT5:
					return D3DFMT_DXT5;
				default:
					return D3DFMT_UNKNOWN;
				}
			}
			//---------------------------------------------------------------------------
			static uint32_t CalculateDataWidth(const uint32_t width, Texture::PixelFormat format)
			{
				switch (format)
				{
				case Texture::PixelFormat::RGBA:
					return width * 4;
				case Texture::PixelFormat::RGB:
					return width * 3;
				case Texture::PixelFormat::RGB_565:
				case Texture::PixelFormat::RGBA_4444:
					return width * 2;
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
			static void BlitRGBToBGRSurface(const uint8_t *src, uint8_t *dst, const SizeF &size)
			{
				for (auto i = 0; i < size.Height; ++i)
				{
					for (auto j = 0; j < size.Width; ++j)
					{
						*dst++ = src[2];
						*dst++ = src[1];
						*dst++ = src[0];
						src += 3;
					}
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
			//---------------------------------------------------------------------------
			void BlitD3DCOLORSurfaceToRGBA(const uint32_t *src, uint32_t *dst, const SizeF &size, uint32_t pitch)
			{
				for (auto i = 0; i < size.Height; ++i)
				{
					for (auto j = 0; j < size.Width; ++j)
					{
						auto pixel = src[j];
						auto tmp = pixel & 0x00FF00FF;
						dst[j] = pixel & 0xFF00FF00 | (tmp << 16) | (tmp >> 16);
					}

					src += pitch / sizeof(uint32_t);
					dst += static_cast<uint32_t>(size.Width);
				}
			}
			//---------------------------------------------------------------------------
			class PixelBuffer
			{
			public:
				PixelBuffer(const void *data, const SizeF &size, Texture::PixelFormat format)
					: sourceBuffer(static_cast<const uint8_t*>(data)),
					pitch(CalculateDataWidth(static_cast<size_t>(size.Width), format))
				{
					if (format == Texture::PixelFormat::RGBA || format == Texture::PixelFormat::RGB)
					{
						workBuffer.resize(pitch * static_cast<size_t>(size.Height));

						if (format == Texture::PixelFormat::RGBA)
						{
							BlitRGBAToD3DCOLORSurface(reinterpret_cast<const uint32_t*>(sourceBuffer), reinterpret_cast<uint32_t*>(workBuffer.data()), size, pitch);
						}
						else
						{
							BlitRGBToBGRSurface(sourceBuffer, workBuffer.data(), size);
						}
					}
				}
				//---------------------------------------------------------------------------
				size_t GetPitch() const
				{
					return pitch;
				}
				//---------------------------------------------------------------------------
				const uint8_t* GetPixelData() const
				{
					return !workBuffer.empty() ? workBuffer.data() : sourceBuffer;
				}
				//---------------------------------------------------------------------------

			private:
				const uint8_t* sourceBuffer;
				std::vector<uint8_t> workBuffer;
				uint32_t pitch;
			};
		}
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D9Texture::Direct3D9Texture(Direct3D9Renderer &_owner)
			: owner(_owner),
			  texture(nullptr),
			  size(0.0f, 0.0f),
			  dataSize(0.0f, 0.0f),
			  texelScaling(0.0f, 0.0f),
			  savedSurfaceDescValid(false)
		{

		}
		//---------------------------------------------------------------------------
		Direct3D9Texture::Direct3D9Texture(Direct3D9Renderer &_owner, const Misc::AnsiString &filename)
			: owner(_owner),
			  texture(nullptr),
			  texelScaling(0.0f, 0.0f),
			  savedSurfaceDescValid(false)
		{
			LoadFromFile(filename);
		}
		//---------------------------------------------------------------------------
		Direct3D9Texture::Direct3D9Texture(Direct3D9Renderer &_owner, const SizeF &_size)
			: owner(_owner),
			  texture(nullptr),
			  size(0.0f, 0.0f),
			  dataSize(size),
			  texelScaling(0.0f, 0.0f),
			  savedSurfaceDescValid(false)
		{
			CreateDirect3D9Texture(_size, D3DFMT_A8R8G8B8);
		}
		//---------------------------------------------------------------------------
		Direct3D9Texture::~Direct3D9Texture()
		{
			CleanupDirect3D9Texture();
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		void Direct3D9Texture::SetDirect3D9Texture(LPDIRECT3DTEXTURE9 tex)
		{
			if (texture != tex)
			{
				CleanupDirect3D9Texture();
				dataSize.Width = dataSize.Height = 0;

				texture = tex;
				if (texture)
					texture->AddRef();
			}

			UpdateTextureSize();
			dataSize = size;
			UpdateCachedScaleValues();
		}
		//---------------------------------------------------------------------------
		LPDIRECT3DTEXTURE9 Direct3D9Texture::GetDirect3D9Texture() const
		{
			return texture;
		}
		//---------------------------------------------------------------------------
		const SizeF& Direct3D9Texture::GetSize() const
		{
			return size;
		}
		//---------------------------------------------------------------------------
		const SizeF& Direct3D9Texture::GetOriginalDataSize() const
		{
			return dataSize;
		}
		//---------------------------------------------------------------------------
		const std::pair<float, float>& Direct3D9Texture::GetTexelScaling() const
		{
			return texelScaling;
		}
		//---------------------------------------------------------------------------
		void Direct3D9Texture::SetOriginalDataSize(const SizeF& size)
		{
			dataSize = size;
			UpdateCachedScaleValues();
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Direct3D9Texture::LoadFromFile(const Misc::AnsiString &filename)
		{
			auto imageData = LoadImageFromFileToRGBABuffer(filename);
			LoadFromMemory(imageData.Data.data(), imageData.Size, PixelFormat::RGBA);
		}
		//---------------------------------------------------------------------------
		void Direct3D9Texture::LoadFromMemory(const void *buffer, const SizeF &size, PixelFormat pixelFormat)
		{
			if (!IsPixelFormatSupported(pixelFormat))
			{
				throw Misc::NotSupportedException();
			}

			auto d3dFormat = ToD3DPixelFormat(pixelFormat);
			CreateDirect3D9Texture(size, d3dFormat);

			auto surface = GetTextureSurface();
			const PixelBuffer pixelBuffer(buffer, size, pixelFormat);

			const RECT rect = { 0, 0, static_cast<LONG>(size.Width), static_cast<LONG>(size.Height) };

			auto hr = D3DXLoadSurfaceFromMemory(surface, nullptr, nullptr, pixelBuffer.GetPixelData(), d3dFormat == D3DFMT_X8R8G8B8 ? D3DFMT_R8G8B8 : d3dFormat, pixelBuffer.GetPitch(), nullptr, &rect, D3DX_FILTER_NONE, 0);

			surface->Release();

			if (FAILED(hr))
			{
				throw Misc::Exception();
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D9Texture::CreateDirect3D9Texture(const SizeF &size, D3DFORMAT format)
		{
			CleanupDirect3D9Texture();

			auto textureSize = owner.GetAdjustedSize(size);

			if (FAILED(D3DXCreateTexture(owner.GetDevice(), static_cast<UINT>(textureSize.Width), static_cast<UINT>(textureSize.Height), 1, 0, format, D3DPOOL_MANAGED, &texture)))
			{
				throw Misc::Exception();
			}

			dataSize = size;
			UpdateTextureSize();
			UpdateCachedScaleValues();
		}
		//---------------------------------------------------------------------------
		IDirect3DSurface9* Direct3D9Texture::GetTextureSurface() const
		{
			LPDIRECT3DSURFACE9 surface;
			if (FAILED(texture->GetSurfaceLevel(0, &surface)))
			{
				throw Misc::Exception();
			}

			return surface;
		}
		//---------------------------------------------------------------------------
		void Direct3D9Texture::CleanupDirect3D9Texture()
		{
			if (texture)
			{
				texture->Release();
				texture = nullptr;
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D9Texture::UpdateCachedScaleValues()
		{
			const float orgW = dataSize.Width;
			const float texW = size.Width;

			texelScaling.first = 1.0f / ((orgW == texW) ? orgW : texW);

			const float orgH = dataSize.Height;
			const float texH = size.Height;

			texelScaling.second = 1.0f / ((orgH == texH) ? orgH : texH);
		}
		//---------------------------------------------------------------------------
		void Direct3D9Texture::UpdateTextureSize()
		{
			D3DSURFACE_DESC description;
			if (texture && SUCCEEDED(texture->GetLevelDesc(0, &description)))
			{
				size.Width  = static_cast<float>(description.Width);
				size.Height = static_cast<float>(description.Height);
			}
			else
			{
				size = dataSize;
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D9Texture::PreD3DReset()
		{
			if (savedSurfaceDescValid || !texture)
			{
				return;
			}

			texture->GetLevelDesc(0, &savedSurfaceDescription);

			if (savedSurfaceDescription.Pool == D3DPOOL_MANAGED)
			{
				return;
			}

			texture->Release();
			texture = nullptr;
			savedSurfaceDescValid = true;
		}
		//---------------------------------------------------------------------------
		void Direct3D9Texture::PostD3DReset()
		{
			if (!savedSurfaceDescValid)
			{
				return;
			}

			owner.GetDevice()->CreateTexture(savedSurfaceDescription.Width, savedSurfaceDescription.Height, 1, savedSurfaceDescription.Usage, savedSurfaceDescription.Format, savedSurfaceDescription.Pool, &texture, 0);

			savedSurfaceDescValid = false;
		}
		//---------------------------------------------------------------------------
		bool Direct3D9Texture::IsPixelFormatSupported(const PixelFormat format) const
		{
			D3DDEVICE_CREATION_PARAMETERS parameters;
			owner.GetDevice()->GetCreationParameters(&parameters);

			LPDIRECT3D9 d3d;
			owner.GetDevice()->GetDirect3D(&d3d);

			D3DDISPLAYMODE displayMode;
			d3d->GetAdapterDisplayMode(parameters.AdapterOrdinal, &displayMode);

			auto d3dFormat = ToD3DPixelFormat(format);

			if (d3dFormat == D3DFMT_UNKNOWN)
			{
				return false;
			}

			return SUCCEEDED(d3d->CheckDeviceFormat(parameters.AdapterOrdinal, parameters.DeviceType, displayMode.Format, 0, D3DRTYPE_TEXTURE, d3dFormat));
		}
		//---------------------------------------------------------------------------
	}
}
