/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_DIRECT3D8TEXTURETARGET_HPP
#define OSHGUI_DRAWING_DIRECT3D8TEXTURETARGET_HPP

#include "Direct3D8RenderTarget.hpp"
#include "../TextureTarget.hpp"

#include <d3d8.h>

namespace OSHGui
{
	namespace Drawing
	{
		class Direct3D8Texture;

		/**
		 * Die Direct3D8 Variante des TextureTarget.
		 */
		class OSHGUI_EXPORT Direct3D8TextureTarget : public Direct3D8RenderTarget<TextureTarget>
		{
		public:
			/**
			 * Konstruktor der Klasse.
			 *
			 * \param owner Renderer Objekt, das das RenderTarget erstellt hat
			 */
			Direct3D8TextureTarget(Direct3D8Renderer& owner);
			/**
			 * Destruktor der Klasse.
			 */
			virtual ~Direct3D8TextureTarget();

			/**
			 * Hilfsfunktion, die vor einem D3D Reset vom Renderer aufgerufen wird.
			 */
			void PreD3DReset();
			/**
			 * Hilfsfunktion, die nach einem D3D Reset vom Renderer aufgerufen wird.
			 */
			void PostD3DReset();

			virtual void Activate() override;
			virtual void Deactivate() override;
			
			virtual bool IsImageryCache() const override;
			
			virtual void Clear() override;
			virtual TexturePtr GetTexture() const override;
			virtual void DeclareRenderSize(const SizeF& size) override;
			virtual bool IsRenderingInverted() const override;

		protected:
			static const float DefaultSize;

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

			LPDIRECT3DTEXTURE8 d3d8Texture;
			LPDIRECT3DSURFACE8 surface;
			
			std::shared_ptr<Direct3D8Texture> texture;
			
			LPDIRECT3DSURFACE8 surfaceBackup;
		};
	}
}

#endif