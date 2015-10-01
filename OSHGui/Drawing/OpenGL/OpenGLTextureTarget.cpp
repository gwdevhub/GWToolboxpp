#include <GL/glew.h>
#include "OpenGLTextureTarget.hpp"
#include "OpenGLRenderer.hpp"
#include "OpenGLTexture.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		const float OpenGLTextureTarget::DefaultSize = 128.0f;
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		OpenGLTextureTarget::OpenGLTextureTarget(OpenGLRenderer &owner)
			: OpenGLRenderTarget<TextureTarget>(owner)
		{
			if (!GLEW_EXT_framebuffer_object)
			{
				throw;
			}

			CreateTexture();
			
			InitialiseRenderTexture();
			
			DeclareRenderSize(SizeF(DefaultSize, DefaultSize));
		}
		//---------------------------------------------------------------------------
		OpenGLTextureTarget::~OpenGLTextureTarget()
		{
			glDeleteFramebuffersEXT(1, &frameBuffer);
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		void OpenGLTextureTarget::DeclareRenderSize(const SizeF &size)
		{
			if (area.GetWidth() >= size.Width && area.GetHeight() >= size.Height)
			{
				return;
			}

			SetArea(RectangleF(area.GetLocation(), owner.GetAdjustedSize(size)));
			ResizeRenderTexture();
			Clear();
		}
		//---------------------------------------------------------------------------
		bool OpenGLTextureTarget::IsImageryCache() const
		{
			return true;
		}
		//---------------------------------------------------------------------------
		TexturePtr OpenGLTextureTarget::GetTexture() const
		{
			return oglTexture;
		}
		//---------------------------------------------------------------------------
		bool OpenGLTextureTarget::IsRenderingInverted() const
		{
			return true;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void OpenGLTextureTarget::CreateTexture()
		{
			oglTexture = std::static_pointer_cast<OpenGLTexture>(owner.CreateTexture());
		}
		//---------------------------------------------------------------------------
		void OpenGLTextureTarget::Activate()
		{
			glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, reinterpret_cast<GLint*>(&previousFrameBuffer));
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frameBuffer);

			OpenGLRenderTarget::Activate();
		}
		//---------------------------------------------------------------------------
		void OpenGLTextureTarget::Deactivate()
		{
			OpenGLRenderTarget::Deactivate();

			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, previousFrameBuffer);
		}
		//---------------------------------------------------------------------------
		void OpenGLTextureTarget::Clear()
		{
			if (area.GetWidth() < 1 || area.GetHeight() < 1)
			{
				return;
			}
			
			GLfloat oldColor[4];
			glGetFloatv(GL_COLOR_CLEAR_VALUE, oldColor);

			GLuint previousFBO = 0;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, reinterpret_cast<GLint*>(&previousFBO));

			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frameBuffer);
			
			glClearColor(0,0,0,0);
			glClear(GL_COLOR_BUFFER_BIT);
			
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, previousFBO);

			glClearColor(oldColor[0], oldColor[1], oldColor[2], oldColor[3]);
		}
		//---------------------------------------------------------------------------
		void OpenGLTextureTarget::InitialiseRenderTexture()
		{
			GLuint oldTexture;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, reinterpret_cast<GLint*>(&oldTexture));

			glGenFramebuffersEXT(1, &frameBuffer);

			GLuint previousFBO = 0;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, reinterpret_cast<GLint*>(&previousFBO));

			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frameBuffer);

			glGenTextures(1, &texture);
			glBindTexture(GL_TEXTURE_2D, texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(DefaultSize), static_cast<GLsizei>(DefaultSize), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
			glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texture, 0);

			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, previousFBO);

			oglTexture->SetOpenGLTexture(texture);
			oglTexture->SetOriginalDataSize(area.GetSize());

			glBindTexture(GL_TEXTURE_2D, oldTexture);
		}
		//---------------------------------------------------------------------------
		void OpenGLTextureTarget::ResizeRenderTexture()
		{
			GLuint oldTexture;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, reinterpret_cast<GLint*>(&oldTexture));

			auto sz = area.GetSize();
			if (sz.Width < 1.0f || sz.Height < 1.0f)
			{
				sz.Width = 1.0f;
				sz.Height = 1.0f;
			}

			glBindTexture(GL_TEXTURE_2D, texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(sz.Width), static_cast<GLsizei>(sz.Height), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
			Clear();

			oglTexture->SetOpenGLTexture(texture);
			oglTexture->SetOriginalDataSize(sz);

			glBindTexture(GL_TEXTURE_2D, oldTexture);
		}
		//---------------------------------------------------------------------------
		void OpenGLTextureTarget::PreReset()
		{
			glDeleteFramebuffersEXT(1, &frameBuffer);
			frameBuffer = 0;

			if (oglTexture)
			{
				texture = 0;
				oglTexture = nullptr;
			}
		}
		//---------------------------------------------------------------------------
		void OpenGLTextureTarget::PostReset()
		{
			if (!oglTexture)
			{
				CreateTexture();
			}

			InitialiseRenderTexture();
			ResizeRenderTexture();
		}
		//---------------------------------------------------------------------------
	}
}
