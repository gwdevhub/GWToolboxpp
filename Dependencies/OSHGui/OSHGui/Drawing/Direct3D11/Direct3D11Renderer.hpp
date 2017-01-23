/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_DIRECT3D11RENDERER_HPP
#define OSHGUI_DRAWING_DIRECT3D11RENDERER_HPP

#include "../Renderer.hpp"
#include "Direct3D11StateBlock.hpp"

#include <vector>
#include <map>

struct ID3DX11Effect;
struct ID3DX11EffectTechnique;
struct ID3DX11EffectShaderResourceVariable;
struct ID3DX11EffectMatrixVariable;
struct ID3DX11EffectScalarVariable;
struct ID3D11ShaderResourceView;

#include <d3d11.h>
#include <d3dx11.h>
#include <d3dx10.h>

namespace OSHGui
{
	namespace Drawing
	{
		class Direct3D11Texture;
		class Direct3D11TextureTarget;
		class Direct3D11GeometryBuffer;

		/**
		 * Die Direct3D11 Variante des Renderer.
		 */
		class OSHGUI_EXPORT Direct3D11Renderer : public Renderer
		{
		public:
			struct IDevice11
			{
				IDevice11(ID3D11Device *device, ID3D11DeviceContext *context)
					: Device(device),
					  Context(context)
				{

				}

				ID3D11Device *Device;
				ID3D11DeviceContext *Context;
			};

			/**
			 * Konstruktor der Klasse.
			 *
			 * \param device Direct3D11 Device
			 * \param context Direct3D11 Context
			 */
			Direct3D11Renderer(ID3D11Device *device, ID3D11DeviceContext *context);
			/**
			 * Destruktor der Klasse.
			 */
			~Direct3D11Renderer();

			/**
			 * Ruft das Direct3D11 Device ab.
			 */
			IDevice11& GetDevice();

			void BindTechniquePass(const bool clipped);
			void SetCurrentTextureShaderResource(ID3D11ShaderResourceView *srv);
			void SetProjectionMatrix(D3DXMATRIX &matrix);
			void SetWorldMatrix(D3DXMATRIX &matrix);

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

			IDevice11 device;
			
			SizeF displaySize;
			PointF displayDPI;
			
			RenderTargetPtr defaultTarget;

			Direct3D11StateBlock stateBlock;
			
			ID3DX11Effect *effect;
			ID3DX11EffectTechnique *clippedTechnique;
			ID3DX11EffectTechnique *unclippedTechnique;
			ID3D11InputLayout *inputLayout;
			
			ID3DX11EffectMatrixVariable *worldMatrixVariable;
			ID3DX11EffectMatrixVariable *projectionMatrixVariable;
			ID3DX11EffectShaderResourceVariable *textureVariable;
			ID3DX11EffectScalarVariable *useTextureVariable;
		};
	}
}

#endif