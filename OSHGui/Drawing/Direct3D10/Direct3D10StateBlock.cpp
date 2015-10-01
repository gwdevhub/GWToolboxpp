#include "Direct3D10StateBlock.hpp"

#include <cstring>

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D10StateBlock::Direct3D10StateBlock(ID3D10Device *device)
			: _device(device),
			_primitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_UNDEFINED),
			_inputLayout(nullptr),
			_blendState(nullptr),
			_sampleMask(0xffffffff),
			_depthStencilState(nullptr),
			_stencilRef(0),
			_rasterizerState(nullptr),
			_VS(nullptr),
			_GS(nullptr),
			//m_pGSSRV(NULL),
			_PS(nullptr),
			_VB(nullptr),
			_vertexStride(0),
			_vertexOffset(0),
			_indexBuffer(nullptr),
			_indexFormat(DXGI_FORMAT_UNKNOWN),
			_indexOffset(0),
			_depthView(nullptr),
			_numViewports(D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)
		{
			_device->AddRef();

			std::memset(_blendFactor, 0, sizeof(_blendFactor));
			std::memset(_VSConstantBuffer, 0, sizeof(_VSConstantBuffer));
			std::memset(_GSConstantBuffer, 0, sizeof(_GSConstantBuffer));
			std::memset(_samplerState, 0, sizeof(_samplerState));
			std::memset(_PSSRV, 0, sizeof(_PSSRV));
			std::memset(_constantBuffers, 0, sizeof(_constantBuffers));
			std::memset(_RTView, 0, sizeof(_RTView));
		}
		//---------------------------------------------------------------------------
		Direct3D10StateBlock::~Direct3D10StateBlock()
		{
			Release();

			_device->Release();
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Direct3D10StateBlock::Capture()
		{
			_device->IAGetPrimitiveTopology(&_primitiveTopology);
			_device->IAGetInputLayout(&_inputLayout);

			_device->OMGetBlendState(&_blendState, _blendFactor, &_sampleMask);
			_device->OMGetDepthStencilState(&_depthStencilState, &_stencilRef);

			_device->RSGetState(&_rasterizerState);

			_device->VSGetShader(&_VS);
			_device->VSGetConstantBuffers(0, 3, _VSConstantBuffer);

			_device->PSGetShader(&_PS);
			_device->PSGetShaderResources(0, 9, _PSSRV);
			_device->PSGetSamplers(0, 3, _samplerState);
			_device->PSGetConstantBuffers(0, 2, _constantBuffers);

			_device->OMGetRenderTargets(1, _RTView, &_depthView);
			_device->RSGetViewports(&_numViewports, _RSViewports);

			_device->GSGetShader(&_GS);
			_device->GSGetConstantBuffers(0, 3, _GSConstantBuffer);

			_device->IAGetVertexBuffers(0, 1, &_VB, &_vertexStride, &_vertexOffset);
			_device->IAGetIndexBuffer(&_indexBuffer, &_indexFormat, &_indexOffset);
		}
		//---------------------------------------------------------------------------
		void Direct3D10StateBlock::Apply()
		{
			_device->IASetPrimitiveTopology(_primitiveTopology);
			_device->IASetInputLayout(_inputLayout);

			_device->RSSetState(_rasterizerState);

			_device->VSSetShader(_VS);
			_device->VSSetConstantBuffers(0, 3, _VSConstantBuffer);

			_device->PSSetShader(_PS);
			_device->PSSetShaderResources(0, 9, _PSSRV);
			_device->PSSetSamplers(0, 3, _samplerState);
			_device->PSSetConstantBuffers(0, 2, _constantBuffers);

			_device->OMSetRenderTargets(1, _RTView, _depthView);
			_device->OMSetBlendState(_blendState, _blendFactor, _sampleMask);
			_device->OMSetDepthStencilState(_depthStencilState, _stencilRef);
			_device->RSSetViewports(_numViewports, _RSViewports);

			_device->GSSetShader(_GS);
			_device->GSSetConstantBuffers(0, 3, _GSConstantBuffer);

			_device->IASetVertexBuffers(0, 1, &_VB, &_vertexStride, &_vertexOffset);
			_device->IASetIndexBuffer(_indexBuffer, _indexFormat, _indexOffset);
		}
		//---------------------------------------------------------------------------
		void Direct3D10StateBlock::Release()
		{
			#define Release_(_1)\
			if (_1 != nullptr) { _1->Release(); _1 = nullptr; }
			#define MultiRelease_(_1, count)\
			for (auto i = 0; i < count; ++i)\
				Release_(_1[i]);

			Release_(_inputLayout);

			Release_(_rasterizerState);

			Release_(_VS);
			MultiRelease_(_VSConstantBuffer, 3);

			Release_(_PS);
			MultiRelease_(_PSSRV, 9);
			MultiRelease_(_samplerState, 3);
			MultiRelease_(_constantBuffers, 2);

			MultiRelease_(_RTView, 2);
			Release_(_depthView);

			Release_(_blendState);
			Release_(_depthStencilState);

			Release_(_GS);
			MultiRelease_(_GSConstantBuffer, 3);

			Release_(_VB);
			Release_(_indexBuffer);
		}
		//---------------------------------------------------------------------------
	}
}
