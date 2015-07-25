/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_DIRECT3D7TEXTURE_HPP
#define OSHGUI_DRAWING_DIRECT3D7TEXTURE_HPP

#include "../Texture.hpp"
#include "Direct3D7Renderer.hpp"

#include "../Vertex.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		/**
		 * Die Direct3D7 Variante der Texture.
		 */
		class OSHGUI_EXPORT Direct3D7Texture : public Texture
		{
		public:
			virtual ~Direct3D7Texture();

			/**
			 * Legt die Direct3D7 Textur fest, die diese Klasse verwendet.
			 *
			 * \param texture Zeiger zur Textur
			 */
			void SetDirect3D7Texture(LPDIRECTDRAWSURFACE7 texture);

			/**
			 * Ruft die Direct3D7 Textur ab, die diese Klasse verwendet.
			 *
			 * \return Zeiger zur Textur
			 */
			LPDIRECTDRAWSURFACE7 GetDirect3D7Texture() const;

			/**
			 * Legt die ungepaddete Originalgröße der Texturdaten fest.
			 * 
			 * \param size Originalgröße
			 */
			void SetOriginalDataSize(const SizeF& size);

			/**
			 * Hilfsfunktion, die vor einem D3D Reset vom Renderer aufgerufen wird.
			 */
			void PreD3DReset();
			/**
			 * Hilfsfunktion, die nach einem D3D Reset vom Renderer aufgerufen wird.
			 */
			void PostD3DReset();

			virtual const SizeF& GetSize() const override;
			virtual const SizeF& GetOriginalDataSize() const override;
			virtual const std::pair<float, float>& GetTexelScaling() const override;
			virtual void LoadFromFile(const Misc::AnsiString &filename) override;
			virtual void LoadFromMemory(const void *buffer, const SizeF &size, PixelFormat pixelFormat) override;
			virtual bool IsPixelFormatSupported(const PixelFormat format) const override;

		protected:
			// Friends (to allow construction and destruction)
			friend TexturePtr Direct3D7Renderer::CreateTexture();
			friend TexturePtr Direct3D7Renderer::CreateTexture(const Misc::AnsiString&);
			friend TexturePtr Direct3D7Renderer::CreateTexture(const SizeF&);

			/**
			 * Erstellt eine leere Textur.
			 *
			 * \param owner Renderer Objekt, das die Textur erstellt hat
			 */
			Direct3D7Texture(Direct3D7Renderer &owner);
			/**
			 * Erstellt eine Textur aus einer Datei.
			 *
			 * \param owner Renderer Objekt, das die Textur erstellt hat
			 * \param path Pfad zur Datei
			 */
			Direct3D7Texture(Direct3D7Renderer &owner, const Misc::AnsiString &path);
			/**
			 * Erstellt eine leere Textur mit der entsprechenden Größe.
			 *
			 * \param owner Renderer Objekt, das die Textur erstellt hat
			 * \param size Größe der Textur
			 */
			Direct3D7Texture(Direct3D7Renderer &owner, const SizeF &size);

			/**
			 * Erzeugt intern die verwendete Textur.
			 *
			 * \param size Größe der Textur
			 * \param format D3DFORMAT der Textur
			 */
			void CreateDirect3D7Texture(const SizeF &size);
			/**
			 * Gibt die intern verwendete Textur frei.
			 */
			void CleanupDirect3D7Texture();
			
			/**
			 * Aktuallisiert die Skalierungswerte für die Pixel-Texel Berechnungen.
			 */
			void UpdateCachedScaleValues();
			/**
			 * Aktuallisiert die Größe der Textur.
			 */
			void UpdateTextureSize();

			Direct3D7Renderer& owner;
			
			LPDIRECTDRAWSURFACE7 texture;
			
			SizeF size;
			SizeF dataSize;
			
			std::pair<float, float> texelScaling;
		};
	}
}

#endif