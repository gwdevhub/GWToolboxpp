#include <algorithm>
#include <GL/glew.h>

#include "OpenGLRenderer.hpp"
#include "OpenGLTexture.hpp"
#include "OpenGLGeometryBuffer.hpp"
#include "OpenGLRenderTarget.hpp"
#include "OpenGLViewportTarget.hpp"
#include "OpenGLTextureTarget.hpp"
#include "../../Misc/Exceptions.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		PFNGLACTIVETEXTUREPROC ActiveTexture;
		PFNGLCLIENTACTIVETEXTUREPROC ClientActiveTexture;
		void APIENTRY activeTextureDummy(GLenum) { }
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		OpenGLRenderer::OpenGLRenderer()
			: displaySize(GetViewportSize()),
			  displayDPI(96, 96)
		{
			GLenum err = glewInit();
			if (GLEW_OK != err)
			{
				throw;
			}

			if (GLEW_VERSION_1_3)
			{
				ActiveTexture = glActiveTexture;
				ClientActiveTexture = glClientActiveTexture;
			}
			else if (GLEW_ARB_multitexture)
			{
				ActiveTexture = glActiveTextureARB;
				ClientActiveTexture = glClientActiveTextureARB;
			}
			else
			{
				ActiveTexture = activeTextureDummy;
				ClientActiveTexture = activeTextureDummy;
			}

			GLint max_tex_size;
			glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex_size);
			maxTextureSize = max_tex_size;

			defaultTarget = std::make_shared<OpenGLViewportTarget>(*this);
		}
		//---------------------------------------------------------------------------
		OpenGLRenderer::~OpenGLRenderer()
		{
			
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		RenderTargetPtr& OpenGLRenderer::GetDefaultRenderTarget()
		{
			return defaultTarget;
		}
		//---------------------------------------------------------------------------
		void OpenGLRenderer::SetDisplaySize(const SizeF &size)
		{
			if (size != displaySize)
			{
				displaySize = size;

				auto area = defaultTarget->GetArea();
				area.SetSize(size);
				defaultTarget->SetArea(area);
			}
		}
		//---------------------------------------------------------------------------
		const SizeF& OpenGLRenderer::GetDisplaySize() const
		{
			return displaySize;
		}
		//---------------------------------------------------------------------------
		const PointF& OpenGLRenderer::GetDisplayDPI() const
		{
			return displayDPI;
		}
		//---------------------------------------------------------------------------
		uint32_t OpenGLRenderer::GetMaximumTextureSize() const
		{
			return maxTextureSize;
		}
		//---------------------------------------------------------------------------
		SizeF OpenGLRenderer::GetViewportSize()
		{
			GLint vp[4];
			glGetIntegerv(GL_VIEWPORT, vp);
			
			return SizeF(static_cast<float>(vp[2]), static_cast<float>(vp[3]));
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		GeometryBufferPtr OpenGLRenderer::CreateGeometryBuffer()
		{
			return std::make_shared<OpenGLGeometryBuffer>(*this);
		}
		//---------------------------------------------------------------------------
		TextureTargetPtr OpenGLRenderer::CreateTextureTarget()
		{
			auto textureTarget = std::make_shared<OpenGLTextureTarget>(*this);
			textureTargets.emplace_back(textureTarget);
			return textureTarget;
		}
		//---------------------------------------------------------------------------
		TexturePtr OpenGLRenderer::CreateTexture()
		{
			auto texture = std::shared_ptr<OpenGLTexture>(new OpenGLTexture(*this));
			textures.emplace_back(texture);
			return texture;
		}
		//---------------------------------------------------------------------------
		TexturePtr OpenGLRenderer::CreateTexture(const Misc::AnsiString &filename)
		{
			auto texture = std::shared_ptr<OpenGLTexture>(new OpenGLTexture(*this, filename));
			textures.emplace_back(texture);
			return texture;
		}
		//---------------------------------------------------------------------------
		TexturePtr OpenGLRenderer::CreateTexture(const SizeF &size)
		{
			auto texture = std::shared_ptr<OpenGLTexture>(new OpenGLTexture(*this, size));
			textures.emplace_back(texture);
			return texture;
		}
		//---------------------------------------------------------------------------
		SizeF OpenGLRenderer::GetAdjustedSize(const SizeF &sz)
		{
			auto s(sz);

			if (!GLEW_ARB_texture_non_power_of_two)
			{
				s.Width  = GetSizeNextPOT(sz.Width);
				s.Height = GetSizeNextPOT(sz.Height);
			}

			return s;
		}
		//---------------------------------------------------------------------------
		float OpenGLRenderer::GetSizeNextPOT(float _size) const
		{
			uint32_t size = _size;

			if ((size & (size - 1)) || !size)
			{
				auto log = 0;

				while (size >>= 1)
				{
					++log;
				}

				size = (2 << log);
			}

			return size;
		}
		//---------------------------------------------------------------------------
		void OpenGLRenderer::BeginRendering()
		{
			glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
			glPushAttrib(GL_ALL_ATTRIB_BITS);

			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();

			//glEnable(GL_SCISSOR_TEST);
			glEnable(GL_TEXTURE_2D);
			glEnable(GL_BLEND);

			if (GLEW_VERSION_1_4)
			{
				glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_ONE);
			}
			else if (GLEW_EXT_blend_func_separate)
			{
				glBlendFuncSeparateEXT(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_ONE);
			}
			else
			{
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}

			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glDisableClientState(GL_SECONDARY_COLOR_ARRAY);
			glDisableClientState(GL_INDEX_ARRAY);
			glDisableClientState(GL_NORMAL_ARRAY);
			glDisableClientState(GL_FOG_COORDINATE_ARRAY);
			glDisableClientState(GL_EDGE_FLAG_ARRAY);

			glMatrixMode(GL_TEXTURE);
			glPushMatrix();
			glLoadIdentity();

			ActiveTexture(GL_TEXTURE0);
			ClientActiveTexture(GL_TEXTURE0);

			glPolygonMode(GL_FRONT, GL_FILL);
			glPolygonMode(GL_BACK, GL_FILL);

			glDisable(GL_LIGHTING);
			glDisable(GL_FOG);
			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_ALPHA_TEST);

			glDisable(GL_TEXTURE_GEN_S);
			glDisable(GL_TEXTURE_GEN_T);
			glDisable(GL_TEXTURE_GEN_R);

			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
		//---------------------------------------------------------------------------
		void OpenGLRenderer::EndRendering()
		{
			glMatrixMode(GL_TEXTURE);
			glPopMatrix();

			glMatrixMode(GL_PROJECTION);
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);
			glPopMatrix();

			glPopAttrib();
			glPopClientAttrib();
		}
		//---------------------------------------------------------------------------
		void OpenGLRenderer::PreReset()
		{
			RemoveWeakReferences();

			for (auto &textureTarget : textureTargets)
			{
				textureTarget.lock()->PreReset();
			}
			for (auto &texture : textures)
			{
				texture.lock()->PreReset();
			}
		}
		//---------------------------------------------------------------------------
		void OpenGLRenderer::PostReset()
		{
			RemoveWeakReferences();

			for (auto &textureTarget : textureTargets)
			{
				textureTarget.lock()->PostReset();
			}
			for (auto &texture : textures)
			{
				texture.lock()->PostReset();
			}
		}
		//---------------------------------------------------------------------------
		void OpenGLRenderer::RemoveWeakReferences()
		{
			textureTargets.erase(std::remove_if(std::begin(textureTargets), std::end(textureTargets), [](const std::weak_ptr<OpenGLTextureTarget> ptr) { return ptr.expired(); }), std::end(textureTargets));
			textures.erase(std::remove_if(std::begin(textures), std::end(textures), [](const std::weak_ptr<OpenGLTexture> ptr) { return ptr.expired(); }), std::end(textures));
		}
		//---------------------------------------------------------------------------
	}
}