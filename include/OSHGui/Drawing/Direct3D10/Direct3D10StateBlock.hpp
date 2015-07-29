/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_DIRECT3D10STATEBLOCK_HPP
#define OSHGUI_DRAWING_DIRECT3D10STATEBLOCK_HPP

#include <d3d10.h>

namespace OSHGui
{
	namespace Drawing
	{
		/**
		 * Status-Speicher für D3D10
		 */
		class Direct3D10StateBlock
		{
		public:
			/**
			 * Konstruktor der Klasse.
			 *
			 * \param context
			 */
			Direct3D10StateBlock(ID3D10Device *device);
			/**
			 * Destruktor der Klasse.
			 */
			~Direct3D10StateBlock();

			/**
			 * Speichert den Status des Device.
			 */
			void Capture();
			/**
			 * Stellt den Status des Device wieder her.
			 */
			void Apply();

		private:
			void Release();

			ID3D10Device 			    *_device;

			D3D10_PRIMITIVE_TOPOLOGY	 _primitiveTopology;
			ID3D10InputLayout			*_inputLayout;
			ID3D10BlendState			*_blendState;
			FLOAT						 _blendFactor[4];
			UINT						 _sampleMask;
			ID3D10DepthStencilState		*_depthStencilState;
			UINT						 _stencilRef;
			ID3D10RasterizerState		*_rasterizerState;
			ID3D10ShaderResourceView	*_PSSRV[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
			ID3D10SamplerState			*_samplerState[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
			ID3D10Buffer				*_constantBuffers[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
			ID3D10VertexShader			*_VS;
			ID3D10Buffer				*_VSConstantBuffer[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
			ID3D10GeometryShader		*_GS;
			ID3D10Buffer				*_GSConstantBuffer[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
			//ID3D10ShaderResourceView	*m_pGSSRV;
			ID3D10PixelShader			*_PS;
			//ID3D10SamplerState          *m_dSSamplers[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
			ID3D10Buffer				*_VB;
			UINT						 _vertexStride;
			UINT						 _vertexOffset;
			ID3D10Buffer				*_indexBuffer;
			DXGI_FORMAT					 _indexFormat;
			UINT						 _indexOffset;
			ID3D10RenderTargetView      *_RTView[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
			ID3D10DepthStencilView      *_depthView;
			UINT                         _numViewports;
			D3D10_VIEWPORT				 _RSViewports[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		};
	}
}

#endif