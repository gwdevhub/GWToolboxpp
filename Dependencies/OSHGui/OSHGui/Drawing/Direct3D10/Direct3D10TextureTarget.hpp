/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_DIRECT3D10TEXTURETARGET_HPP
#define OSHGUI_DRAWING_DIRECT3D10TEXTURETARGET_HPP

#include "Direct3D10RenderTarget.hpp"
#include "../TextureTarget.hpp"

struct ID3D10Texture2D;
struct ID3D10RenderTargetView;
struct ID3D10DepthStencilView;

namespace OSHGui
{
	namespace Drawing
	{
		class Direct3D10Texture;

		/**
		 * Die Direct3D10 Variante des TextureTarget.
		 */
		class OSHGUI_EXPORT Direct3D10TextureTarget : public Direct3D10RenderTarget<TextureTarget>
		{
		public:
			/**
			 * Konstruktor der Klasse.
			 *
			 * \param owner Renderer Objekt, das das RenderTarget erstellt hat
			 */
			Direct3D10TextureTarget(Direct3D10Renderer& owner);
			/**
			 * Destruktor der Klasse.
			 */
			virtual ~Direct3D10TextureTarget();

			virtual bool IsImageryCache() const override;

			virtual void Activate() override;
			virtual void Deactivate() override;
			
			virtual TexturePtr GetTexture() const override;
			virtual void DeclareRenderSize(const SizeF& size) override;
			virtual bool IsRenderingInverted() const override;
			virtual void Clear() override;

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

			ID3D10Texture2D *d3dTexture;
			ID3D10RenderTargetView *renderTargetView;
			
			std::shared_ptr<Direct3D10Texture> texture;
			
			ID3D10RenderTargetView *renderTargetViewBackup;
			ID3D10DepthStencilView *depthStencilViewBackup;
		};
	}
}

#endif