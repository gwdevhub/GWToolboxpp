/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_DIRECT3D11TEXTURE_HPP
#define OSHGUI_DRAWING_DIRECT3D11TEXTURE_HPP

#include "../Texture.hpp"
#include "Direct3D11Renderer.hpp"

#include "../Vertex.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		/**
		 * Die Direct3D11 Variante der Texture.
		 */
		class OSHGUI_EXPORT Direct3D11Texture : public Texture
		{
		public:
			virtual ~Direct3D11Texture();

			/**
			 * Legt die Direct3D11 Textur fest, die diese Klasse verwendet.
			 *
			 * \param texture Zeiger zur Textur
			 */
			void SetDirect3D11Texture(ID3D11Texture2D *texture);

			/**
			 * Ruft die Direct3D11 Textur ab, die diese Klasse verwendet.
			 *
			 * \return Zeiger zur Textur
			 */
			ID3D11Texture2D* GetDirect3D11Texture() const;
			/**
			 * Ruft den internen D3D10 Shader Resource View der Textur ab.
			 * 
			 * \return Shader Resource View
			 */
			ID3D11ShaderResourceView* GetDirect3DShaderResourceView() const;

			/**
			 * Legt die ungepaddete Originalgröße der Texturdaten fest.
			 * 
			 * \param size Originalgröße
			 */
			void SetOriginalDataSize(const SizeF& size);

			virtual const SizeF& GetSize() const override;
			virtual const SizeF& GetOriginalDataSize() const override;
			virtual const std::pair<float, float>& GetTexelScaling() const override;
			virtual void LoadFromFile(const Misc::AnsiString &filename) override;
			virtual void LoadFromMemory(const void *buffer, const SizeF &size, PixelFormat pixelFormat) override;
			virtual bool IsPixelFormatSupported(const PixelFormat format) const override;

		protected:
			// Friends (to allow construction and destruction)
			friend TexturePtr Direct3D11Renderer::CreateTexture();
			friend TexturePtr Direct3D11Renderer::CreateTexture(const Misc::AnsiString&);
			friend TexturePtr Direct3D11Renderer::CreateTexture(const SizeF&);

			/**
			 * Erstellt eine leere Textur.
			 *
			 * \param owner Renderer Objekt, das die Textur erstellt hat
			 */
			Direct3D11Texture(Direct3D11Renderer::IDevice11 &device);
			/**
			 * Erstellt eine Textur aus einer Datei.
			 *
			 * \param owner Renderer Objekt, das die Textur erstellt hat
			 * \param path Pfad zur Datei
			 */
			Direct3D11Texture(Direct3D11Renderer::IDevice11 &device, const Misc::AnsiString &path);
			/**
			 * Erstellt eine leere Textur mit der entsprechenden Größe.
			 *
			 * \param owner Renderer Objekt, das die Textur erstellt hat
			 * \param size Größe der Textur
			 */
			Direct3D11Texture(Direct3D11Renderer::IDevice11 &device, const SizeF &size);

			/**
			 * Erzeugt intern die verwendete Textur.
			 *
			 * \param size Größe der Textur
			 * \param format D3DFORMAT der Textur
			 */
			void CreateDirect3D11Texture(const SizeF &size, DXGI_FORMAT format, D3D11_SUBRESOURCE_DATA *subresourceData);
			/**
			 * Gibt die intern verwendete Textur frei.
			 */
			void CleanupDirect3D11Texture();
			
			/**
			 * Aktuallisiert die Skalierungswerte für die Pixel-Texel Berechnungen.
			 */
			void UpdateCachedScaleValues();
			/**
			 * Aktuallisiert die Größe der Textur.
			 */
			void UpdateTextureSize();
			/**
			 * Erzeugt die Shader Resource View für die Textur.
			 */
			void InitialiseShaderResourceView();

			Direct3D11Renderer::IDevice11 &device;
			
			ID3D11Texture2D *texture;
			ID3D11ShaderResourceView *resourceView;
			
			SizeF size;
			SizeF dataSize;
			
			std::pair<float, float> texelScaling;
		};
	}
}

#endif