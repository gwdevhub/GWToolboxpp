/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_OPENGLTEXTURETARGET_HPP
#define OSHGUI_DRAWING_OPENGLTEXTURETARGET_HPP

#include "GL.hpp"

#include "OpenGLRenderTarget.hpp"
#include "../TextureTarget.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		class OpenGLTexture;

		/**
		 * Die OpenGL Variante des TextureTarget.
		 */
		class OSHGUI_EXPORT OpenGLTextureTarget : public OpenGLRenderTarget<TextureTarget>
		{
		public:
			/**
			 * Konstruktor der Klasse.
			 *
			 * \param owner Renderer Objekt, das das RenderTarget erstellt hat
			 */
			OpenGLTextureTarget(OpenGLRenderer& owner);
			/**
			 * Destruktor der Klasse.
			 */
			virtual ~OpenGLTextureTarget();

			/**
			* Hilfsfunktion, die vor einem Reset aufgerufen werden muss.
			*/
			void PreReset();
			/**
			* Hilfsfunktion, die nach einem Reset aufgerufen werden muss.
			*/
			void PostReset();

			virtual void Activate() override;
			virtual void Deactivate() override;
			
			virtual bool IsImageryCache() const override;
			
			virtual void Clear() override;
			virtual TexturePtr GetTexture() const override;
			virtual void DeclareRenderSize(const SizeF& size) override;
			virtual bool IsRenderingInverted() const override;

		protected:
			static const float DefaultSize;

			void CreateTexture();

			/**
			 * Erzeugt die Textur, auf die gezeichnet wird.
			 */
			void InitialiseRenderTexture();
			/**
			 * Löscht die Textur, auf die gezeichnet wird.
			 */
			void CleanupRenderTexture();
			/**
			 * Aktuallisiert die Größe der Textur.
			 */
			void ResizeRenderTexture();

			/**
			 * Legt die Textur als Ziel für Zeichenoperationen fest.
			 */
			void EnableRenderTexture();
			/**
			 * Stellt das vorherige Ziel für Zeichenoperationen wieder her.
			 */
			void DisableRenderTexture();

			GLuint texture;
			std::shared_ptr<OpenGLTexture> oglTexture;
			
			GLuint frameBuffer;
			GLuint previousFrameBuffer;
		};
	}
}

#endif