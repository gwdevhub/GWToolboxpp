#include "OpenGLTexture.hpp"
#include "../ImageLoader.hpp"
#include "../../Misc/Exceptions.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		OpenGLTexture::OpenGLTexture(OpenGLRenderer &_owner)
			: owner(_owner),
			  texture(0),
			  dataBuffer(nullptr),
			  size(0.0f, 0.0f),
			  dataSize(0.0f, 0.0f),
			  texelScaling(0.0f, 0.0f)
		{
			CreateOpenGLTexture();
		}
		//---------------------------------------------------------------------------
		OpenGLTexture::OpenGLTexture(OpenGLRenderer &_owner, const Misc::AnsiString &filename)
			: owner(_owner),
			  texture(0),
			  dataBuffer(nullptr),
			  texelScaling(0.0f, 0.0f)
		{
			CreateOpenGLTexture();

			LoadFromFile(filename);
		}
		//---------------------------------------------------------------------------
		OpenGLTexture::OpenGLTexture(OpenGLRenderer &_owner, const SizeF &_size)
			: owner(_owner),
			  texture(0),
			  dataBuffer(nullptr),
			  texelScaling(0.0f, 0.0f)
		{
			CreateOpenGLTexture();
			SetOriginalDataSize(_size);
			SetTextureSizeInternal(_size);
		}
		//---------------------------------------------------------------------------
		OpenGLTexture::~OpenGLTexture()
		{
			CleanupOpenGLTexture();
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		void OpenGLTexture::SetOpenGLTexture(GLuint _texture)
		{
			if (texture != _texture)
			{
				CleanupOpenGLTexture();
				dataSize.Width = dataSize.Height = 0;

				texture = _texture;
			}

			dataSize = size;
			UpdateCachedScaleValues();
		}
		//---------------------------------------------------------------------------
		GLuint OpenGLTexture::GetOpenGLTexture() const
		{
			return texture;
		}
		//---------------------------------------------------------------------------
		const SizeF& OpenGLTexture::GetSize() const
		{
			return size;
		}
		//---------------------------------------------------------------------------
		const SizeF& OpenGLTexture::GetOriginalDataSize() const
		{
			return dataSize;
		}
		//---------------------------------------------------------------------------
		const std::pair<float, float>& OpenGLTexture::GetTexelScaling() const
		{
			return texelScaling;
		}
		//---------------------------------------------------------------------------
		void OpenGLTexture::SetOriginalDataSize(const SizeF& size)
		{
			dataSize = size;
			UpdateCachedScaleValues();
		}
		//---------------------------------------------------------------------------
		void OpenGLTexture::SetTextureSizeInternal(const SizeF& sz)
		{
			const SizeF size = owner.GetAdjustedSize(sz);
			this->size = size;

			GLfloat maxSize;
			glGetFloatv(GL_MAX_TEXTURE_SIZE, &maxSize);
			if (size.Width > maxSize || size.Height > maxSize)
			{
				throw;
			}

			GLuint oldTexture;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, reinterpret_cast<GLint*>(&oldTexture));

			glBindTexture(GL_TEXTURE_2D, texture);

			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(size.Width), static_cast<GLsizei>(size.Height), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

			glBindTexture(GL_TEXTURE_2D, oldTexture);
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void OpenGLTexture::LoadFromFile(const Misc::AnsiString &filename)
		{
			auto imageData = LoadImageFromFileToRGBABuffer(filename);
			dataSize = imageData.Size;
			LoadFromMemory(imageData.Data.data(), imageData.Size, PixelFormat::RGBA);
		}
		//---------------------------------------------------------------------------
		void OpenGLTexture::LoadFromMemory(const void *buffer, const SizeF &bufferSize, PixelFormat pixelFormat)
		{
			SetTextureSizeInternal(bufferSize);

			SetOriginalDataSize(bufferSize);

			BlitFromMemory(buffer, RectangleF(PointF(0, 0), bufferSize));
		}
		//---------------------------------------------------------------------------
		void OpenGLTexture::BlitFromMemory(const void* sourceData, const RectangleF& area)
		{
			GLuint oldTexture;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, reinterpret_cast<GLint*>(&oldTexture));

			glBindTexture(GL_TEXTURE_2D, texture);

			GLint old_pack;
			glGetIntegerv(GL_UNPACK_ALIGNMENT, &old_pack);

			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

			glTexSubImage2D(GL_TEXTURE_2D, 0,
				static_cast<GLint>(area.GetLeft()),
				static_cast<GLint>(area.GetTop()),
				static_cast<GLsizei>(area.GetWidth()),
				static_cast<GLsizei>(area.GetHeight()),
				GL_RGBA, GL_UNSIGNED_BYTE, sourceData);

			glPixelStorei(GL_UNPACK_ALIGNMENT, old_pack);

			glBindTexture(GL_TEXTURE_2D, oldTexture);
		}
		//---------------------------------------------------------------------------
		void OpenGLTexture::BlitToMemory(void* targetData)
		{
			GLuint oldTexture;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, reinterpret_cast<GLint*>(&oldTexture));

			glBindTexture(GL_TEXTURE_2D, texture);

			GLint oldPackAlignment;
			glGetIntegerv(GL_PACK_ALIGNMENT, &oldPackAlignment);

			glPixelStorei(GL_PACK_ALIGNMENT, 1);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, targetData);

			glPixelStorei(GL_PACK_ALIGNMENT, oldPackAlignment);
			
			glBindTexture(GL_TEXTURE_2D, oldTexture);
		}
		//---------------------------------------------------------------------------
		void OpenGLTexture::CreateOpenGLTexture()
		{
			GLuint oldTexture;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, reinterpret_cast<GLint*>(&oldTexture));

			glGenTextures(1, &texture);

			glBindTexture(GL_TEXTURE_2D, texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);

			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

			glBindTexture(GL_TEXTURE_2D, oldTexture);
		}
		//---------------------------------------------------------------------------
		void OpenGLTexture::CleanupOpenGLTexture()
		{
			if (dataBuffer != nullptr)
			{
				delete[] dataBuffer;
				dataBuffer = nullptr;
			}
			else
			{
				glDeleteTextures(1, &texture);
				texture = 0;
			}
		}
		//---------------------------------------------------------------------------
		void OpenGLTexture::UpdateCachedScaleValues()
		{
			const float orgW = dataSize.Width;
			const float texW = size.Width;

			if (orgW == texW && orgW == 0.0f)
			{
				texelScaling.first = 0.0f;
			}
			else
			{
				texelScaling.first = 1.0f / ((orgW == texW) ? orgW : texW);
			}

			const float orgH = dataSize.Height;
			const float texH = size.Height;

			if (orgH == texH && orgH == 0.0f)
			{
				texelScaling.second = 0.0f;
			}
			else
			{
				texelScaling.second = 1.0f / ((orgH == texH) ? orgH : texH);
			}
		}
		//---------------------------------------------------------------------------
		void OpenGLTexture::PreReset()
		{
			if (dataBuffer != nullptr)
			{
				return;
			}

			dataBuffer = new uint8_t[static_cast<int>(4 * size.Width * size.Height)];

			BlitToMemory(dataBuffer);

			glDeleteTextures(1, &texture);
		}
		//---------------------------------------------------------------------------
		void OpenGLTexture::PostReset()
		{
			if (dataBuffer == nullptr)
			{
				return;
			}

			CreateOpenGLTexture();
			SetOriginalDataSize(size);
			SetTextureSizeInternal(size);

			BlitFromMemory(dataBuffer, RectangleF(PointF(0, 0), size));

			delete[] dataBuffer;
			dataBuffer = nullptr;
		}
		//---------------------------------------------------------------------------
		bool OpenGLTexture::IsPixelFormatSupported(const PixelFormat format) const
		{
			switch (format)
			{
				case PixelFormat::RGBA:
				case PixelFormat::RGB:
				case PixelFormat::RGBA_4444:
				case PixelFormat::RGB_565:
					return true;
				case PixelFormat::RGB_DXT1:
				case PixelFormat::RGBA_DXT1:
				case PixelFormat::RGBA_DXT3:
				case PixelFormat::RGBA_DXT5:
				default:
					return false;
			}
		}
		//---------------------------------------------------------------------------
	}
}
