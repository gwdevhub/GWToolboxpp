/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_DIRECT3D7TEXTURETARGET_HPP
#define OSHGUI_DRAWING_DIRECT3D7TEXTURETARGET_HPP

#include "Direct3D7RenderTarget.hpp"
#include "../TextureTarget.hpp"

#include <d3d8.h>

namespace OSHGui
{
	namespace Drawing
	{
		class Direct3D7Texture;

		/**
		 * Die Direct3D7 Variante des TextureTarget.
		 */
		class OSHGUI_EXPORT Direct3D7TextureTarget : public Direct3D7RenderTarget<TextureTarget>
		{
		public:
			/**
			 * Konstruktor der Klasse.
			 *
			 * \param owner Renderer Objekt, das das RenderTarget erstellt hat
			 */
			Direct3D7TextureTarget(Direct3D7Renderer& owner);
			/**
			 * Destruktor der Klasse.
			 */
			virtual ~Direct3D7TextureTarget();

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

			LPDIRECTDRAWSURFACE7 surface;
			
			std::shared_ptr<Direct3D7Texture> texture;
			
			LPDIRECTDRAWSURFACE7 surfaceBackup;
		};
	}
}

#endif