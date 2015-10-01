/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_OPENGLTEXTURE_HPP
#define OSHGUI_DRAWING_OPENGLTEXTURE_HPP

#include "../Texture.hpp"
#include "OpenGLRenderer.hpp"

#include "../Vertex.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		/**
		 * Die OpenGL Variante der Texture.
		 */
		class OSHGUI_EXPORT OpenGLTexture : public Texture
		{
		public:
			virtual ~OpenGLTexture();

			/**
			 * Legt die OpenGL Textur fest, die diese Klasse verwendet.
			 *
			 * \param texture ID der Textur
			 */
			void SetOpenGLTexture(GLuint texture);

			/**
			 * Ruft die OpenGL Textur ab, die diese Klasse verwendet.
			 *
			 * \return ID der Textur
			 */
			GLuint GetOpenGLTexture() const;

			/**
			 * Legt die ungepaddete Originalgröße der Texturdaten fest.
			 * 
			 * \param size Originalgröße
			 */
			void SetOriginalDataSize(const SizeF& size);

			/**
			 * Hilfsfunktion, die vor einem Reset vom Renderer aufgerufen wird.
			 */
			void PreReset();
			/**
			 * Hilfsfunktion, die nach einem Reset vom Renderer aufgerufen wird.
			 */
			void PostReset();

			virtual const SizeF& GetSize() const override;
			virtual const SizeF& GetOriginalDataSize() const override;
			virtual const std::pair<float, float>& GetTexelScaling() const override;
			virtual void LoadFromFile(const Misc::AnsiString &filename) override;
			virtual void LoadFromMemory(const void *buffer, const SizeF &size, PixelFormat pixelFormat) override;
			virtual bool IsPixelFormatSupported(const PixelFormat format) const override;

		protected:
			// Friends (to allow construction and destruction)
			friend TexturePtr OpenGLRenderer::CreateTexture();
			friend TexturePtr OpenGLRenderer::CreateTexture(const Misc::AnsiString&);
			friend TexturePtr OpenGLRenderer::CreateTexture(const SizeF&);

			/**
			 * Erstellt eine leere Textur.
			 *
			 * \param owner Renderer Objekt, das die Textur erstellt hat
			 */
			OpenGLTexture(OpenGLRenderer &owner);
			/**
			 * Erstellt eine Textur aus einer Datei.
			 *
			 * \param owner Renderer Objekt, das die Textur erstellt hat
			 * \param path Pfad zur Datei
			 */
			OpenGLTexture(OpenGLRenderer &owner, const Misc::AnsiString &path);
			/**
			 * Erstellt eine leere Textur mit der entsprechenden Größe.
			 *
			 * \param owner Renderer Objekt, das die Textur erstellt hat
			 * \param size Größe der Textur
			 */
			OpenGLTexture(OpenGLRenderer &owner, const SizeF &size);

			/**
			 * Erzeugt intern die verwendete Textur.
			 */
			virtual void CreateOpenGLTexture();
			/**
			 * Gibt die intern verwendete Textur frei.
			 */
			void CleanupOpenGLTexture();

			void BlitFromMemory(const void* sourceData, const RectangleF& area);
			void BlitToMemory(void* targetData);

			void SetTextureSizeInternal(const SizeF &size);
			
			/**
			 * Aktuallisiert die Skalierungswerte für die Pixel-Texel Berechnungen.
			 */
			void UpdateCachedScaleValues();

			OpenGLRenderer& owner;
			
			GLuint texture;
			uint8_t *dataBuffer;
			
			SizeF size;
			SizeF dataSize;
			
			std::pair<float, float> texelScaling;
		};
	}
}

#endif