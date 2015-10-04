/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_DIRECT3D9TEXTURE_HPP
#define OSHGUI_DRAWING_DIRECT3D9TEXTURE_HPP

#include "../Texture.hpp"
#include "Direct3D9Renderer.hpp"

#include "../Vertex.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		/**
		 * Die Direct3D9 Variante der Texture.
		 */
		class OSHGUI_EXPORT Direct3D9Texture : public Texture
		{
		public:
			virtual ~Direct3D9Texture();

			/**
			 * Legt die Direct3D9 Textur fest, die diese Klasse verwendet.
			 *
			 * \param texture Zeiger zur Textur
			 */
			void SetDirect3D9Texture(LPDIRECT3DTEXTURE9 texture);

			/**
			 * Ruft die Direct3D9 Textur ab, die diese Klasse verwendet.
			 *
			 * \return Zeiger zur Textur
			 */
			LPDIRECT3DTEXTURE9 GetDirect3D9Texture() const;

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
			friend TexturePtr Direct3D9Renderer::CreateTexture();
			friend TexturePtr Direct3D9Renderer::CreateTexture(const Misc::AnsiString&);
			friend TexturePtr Direct3D9Renderer::CreateTexture(const SizeF&);

			/**
			 * Erstellt eine leere Textur.
			 *
			 * \param owner Renderer Objekt, das die Textur erstellt hat
			 */
			Direct3D9Texture(Direct3D9Renderer &owner);
			/**
			 * Erstellt eine Textur aus einer Datei.
			 *
			 * \param owner Renderer Objekt, das die Textur erstellt hat
			 * \param path Pfad zur Datei
			 */
			Direct3D9Texture(Direct3D9Renderer &owner, const Misc::AnsiString &path);
			/**
			 * Erstellt eine leere Textur mit der entsprechenden Größe.
			 *
			 * \param owner Renderer Objekt, das die Textur erstellt hat
			 * \param size Größe der Textur
			 */
			Direct3D9Texture(Direct3D9Renderer &owner, const SizeF &size);

			/**
			 * Erzeugt intern die verwendete Textur.
			 *
			 * \param size Größe der Textur
			 * \param format D3DFORMAT der Textur
			 */
			virtual void CreateDirect3D9Texture(const SizeF &size, D3DFORMAT format);
			/**
			 * Gibt die intern verwendete Textur frei.
			 */
			void CleanupDirect3D9Texture();
			
			/**
			 * Ruft das Direct3D9 Surface der Textur ab.
			 *
			 * \return Direct3D9 Surface der Textur
			 */
			IDirect3DSurface9* GetTextureSurface() const;
			/**
			 * Aktuallisiert die Skalierungswerte für die Pixel-Texel Berechnungen.
			 */
			void UpdateCachedScaleValues();
			/**
			 * Aktuallisiert die Größe der Textur.
			 */
			void UpdateTextureSize();

			Direct3D9Renderer& owner;
			
			LPDIRECT3DTEXTURE9 texture;
			
			SizeF size;
			SizeF dataSize;
			
			std::pair<float, float> texelScaling;
			
			D3DSURFACE_DESC savedSurfaceDescription;
			bool savedSurfaceDescValid;
		};
	}
}

#endif