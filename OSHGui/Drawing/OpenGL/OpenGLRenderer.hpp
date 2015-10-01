/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_OPENGLRENDERER_HPP
#define OSHGUI_DRAWING_OPENGLRENDERER_HPP

#include "../Renderer.hpp"

#include "GL.hpp"
#include <vector>
#include <map>

namespace OSHGui
{
	namespace Drawing
	{
		class OpenGLTexture;
		class OpenGLTextureTarget;
		class OpenGLGeometryBuffer;

		/**
		 * Die OpenGL Variante des Renderer.
		 */
		class OSHGUI_EXPORT OpenGLRenderer : public Renderer
		{
		public:
			/**
			 * Konstruktor der Klasse.
			 */
			OpenGLRenderer();
			~OpenGLRenderer();

			/**
			 * Hilfsfunktion, die vor einem Reset aufgerufen werden muss.
			 */
			void PreReset();
			/**
			 * Hilfsfunktion, die nach einem Reset aufgerufen werden muss.
			 */
			void PostReset();

			/**
			 * Rechnet die Größe in eine unterstützt Größe um.
			 *
			 * \param size
			 * \return umgewandelte Größe
			 */
			SizeF GetAdjustedSize(const SizeF &size);

			virtual RenderTargetPtr& GetDefaultRenderTarget() override;
			virtual GeometryBufferPtr CreateGeometryBuffer() override;
			virtual TextureTargetPtr CreateTextureTarget() override;
			virtual TexturePtr CreateTexture() override;
			virtual TexturePtr CreateTexture(const Misc::AnsiString &filename) override;
			virtual TexturePtr CreateTexture(const SizeF &size) override;

			virtual void BeginRendering() override;
			virtual void EndRendering() override;
			virtual void SetDisplaySize(const SizeF &sz) override;
			virtual const SizeF& GetDisplaySize() const override;
			virtual const PointF& GetDisplayDPI() const override;
			virtual uint32_t GetMaximumTextureSize() const override;

		private:
			/**
			 * Ruft die Größe des Viewport ab.
			 *
			 * \return Größe des Viewport
			 */
			SizeF GetViewportSize();
			/**
			 * Berechnet die nächste Zweierpotenz, falss \a size keine Zweierpotenz ist.
			 *
			 * \param size
			 * \return nächste Zweierpotenz
			 */
			float GetSizeNextPOT(float size) const;

			/**
			 * Entfernt ungültige Referenzen zu Objekten, für die ein Reset ausgeführt werden muss.
			 */
			void RemoveWeakReferences();

			SizeF displaySize;
			PointF displayDPI;
			
			RenderTargetPtr defaultTarget;
			
			typedef std::vector<std::weak_ptr<OpenGLTextureTarget>> TextureTargetList;
			TextureTargetList textureTargets;
			
			typedef std::vector<std::weak_ptr<OpenGLTexture>> TextureList;
			TextureList textures;
			
			uint32_t maxTextureSize;
		};
	}
}

#endif