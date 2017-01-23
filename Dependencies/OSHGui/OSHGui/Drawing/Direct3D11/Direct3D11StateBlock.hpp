/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_DIRECT3D11STATEBLOCK_HPP
#define OSHGUI_DRAWING_DIRECT3D11STATEBLOCK_HPP

#include <d3d11.h>

namespace OSHGui
{
	namespace Drawing
	{
		/**
		 * Status-Speicher für D3D11
		 */
		class Direct3D11StateBlock
		{
		public:
			/**
			 * Konstruktor der Klasse.
			 *
			 * \param context
			 */
			Direct3D11StateBlock(ID3D11DeviceContext *context);
			/**
			 * Destruktor der Klasse.
			 */
			~Direct3D11StateBlock();

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

			ID3D11DeviceContext *_context;

			D3D11_PRIMITIVE_TOPOLOGY	 _primitiveTopology;
			ID3D11InputLayout			*_inputLayout;
			ID3D11BlendState			*_blendState;
			FLOAT						 _blendFactor[4];
			UINT						 _sampleMask;
			ID3D11DepthStencilState		*_depthStencilState;
			UINT						 _stencilRef;
			ID3D11RasterizerState		*_rasterizerState;
			ID3D11ShaderResourceView	*_PSSRV[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
			ID3D11SamplerState			*_samplerState[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
			ID3D11Buffer				*_constantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
			ID3D11VertexShader			*_VS;
			ID3D11ClassInstance			*_VSClassInstances[256];
			UINT						 _numVSClassInstances;
			ID3D11Buffer				*_VSConstantBuffer[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
			ID3D11GeometryShader		*_GS;
			ID3D11ClassInstance			*_GSClassInstances[256];
			UINT						 _numGSClassInstances;
			ID3D11Buffer				*_GSConstantBuffer[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
			ID3D11ShaderResourceView	*_GSSRV;
			ID3D11PixelShader			*_PS;
			ID3D11ClassInstance			*_PSClassInstances[256];
			UINT						 _numPSClassInstances;
			ID3D11HullShader			*_HS;
			ID3D11ClassInstance			*_HSClassInstances[256];
			UINT						 _numHSClassInstances;
			ID3D11DomainShader			*_DS;
			ID3D11SamplerState          *_DSSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
			ID3D11ClassInstance			*_DSClassInstances[256];
			UINT						 _numDSClassInstances;
			ID3D11Buffer				*_VB[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			UINT						 _vertexStride[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			UINT						 _vertexOffset[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			ID3D11Buffer				*_indexBuffer;
			DXGI_FORMAT					 _indexFormat;
			UINT						 _indexOffset;
			ID3D11RenderTargetView      *_RTView[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
			ID3D11DepthStencilView      *_depthView;
			UINT                         _numViewports;
			D3D11_VIEWPORT				 _RSViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		};
	}
}

#endif