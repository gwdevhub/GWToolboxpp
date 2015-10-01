#include "Direct3D11StateBlock.hpp"

#include <cstring>

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D11StateBlock::Direct3D11StateBlock(ID3D11DeviceContext *context)
			: _context(context),
			  _inputLayout(nullptr),
			  _blendState(nullptr),
			  _depthStencilState(nullptr),
			  _rasterizerState(nullptr),
			  _VS(nullptr),
			  _numVSClassInstances(0),
			  _GS(nullptr),
			  _numGSClassInstances(0),
			  _GSSRV(nullptr),
			  _PS(nullptr),
			  _numPSClassInstances(0),
			  _HS(nullptr),
			  _numHSClassInstances(0),
			  _DS(nullptr),
			  _numDSClassInstances(0),
			  _indexBuffer(nullptr),
			  _indexFormat(DXGI_FORMAT_UNKNOWN),
			  _depthView(nullptr),
			  _numViewports(D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)
		{
			_context->AddRef();

			std::memset(_blendFactor, 0, sizeof(_blendFactor));
			std::memset(_VSConstantBuffer, 0, sizeof(_VSConstantBuffer));
			std::memset(_GSConstantBuffer, 0, sizeof(_GSConstantBuffer));
			std::memset(_samplerState, 0, sizeof(_samplerState));
			std::memset(_PSSRV, 0, sizeof(_PSSRV));
			std::memset(_constantBuffers, 0, sizeof(_constantBuffers));
			std::memset(_RTView, 0, sizeof(_RTView));
			std::memset(_DSSamplers, 0, sizeof(_DSSamplers));
			std::memset(_VB, 0, sizeof(_VB));
			std::memset(_vertexStride, 0, sizeof(_vertexStride));
			std::memset(_vertexOffset, 0, sizeof(_vertexOffset));
		}
		//---------------------------------------------------------------------------
		Direct3D11StateBlock::~Direct3D11StateBlock()
		{
			Release();

			_context->Release();
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Direct3D11StateBlock::Capture()
		{
			_context->IAGetPrimitiveTopology(&_primitiveTopology);
			_context->IAGetInputLayout(&_inputLayout);

			_context->OMGetBlendState(&_blendState, _blendFactor, &_sampleMask);
			_context->OMGetDepthStencilState(&_depthStencilState, &_stencilRef);

			_context->RSGetState(&_rasterizerState);

			_numVSClassInstances = 256;
			_context->VSGetShader(&_VS, _VSClassInstances, &_numVSClassInstances);
			_context->VSGetConstantBuffers(0, 4, _VSConstantBuffer);

			_numPSClassInstances = 256;
			_context->PSGetShader(&_PS, _PSClassInstances, &_numPSClassInstances);
			_context->PSGetShaderResources(0, 9, _PSSRV);
			_context->PSGetSamplers(0, 10, _samplerState);
			_context->PSGetConstantBuffers(0, 3, _constantBuffers);
			_context->OMGetRenderTargetsAndUnorderedAccessViews(2, _RTView, &_depthView, 0, 0, nullptr);
			_context->RSGetViewports(&_numViewports, _RSViewports);

			_numGSClassInstances = 256;
			_context->GSGetShader(&_GS, _GSClassInstances, &_numGSClassInstances);
			_context->GSGetConstantBuffers(0, 2, _GSConstantBuffer);

			_context->GSGetShaderResources(0, 1, &_GSSRV);

			_numHSClassInstances = 256;
			_context->HSGetShader(&_HS, _HSClassInstances, &_numHSClassInstances);

			_numDSClassInstances = 256;
			_context->DSGetShader(&_DS, _DSClassInstances, &_numDSClassInstances);
			_context->DSGetSamplers(0, 3, _DSSamplers);

			_context->IAGetVertexBuffers(0, 3, _VB, _vertexStride, _vertexOffset);
			_context->IAGetIndexBuffer(&_indexBuffer, &_indexFormat, &_indexOffset);
		}
		//---------------------------------------------------------------------------
		void Direct3D11StateBlock::Apply()
		{
			_context->IASetPrimitiveTopology(_primitiveTopology);
			_context->IASetInputLayout(_inputLayout);

			_context->RSSetState(_rasterizerState);

			_context->VSSetShader(_VS, _VSClassInstances, _numVSClassInstances);
			_context->VSSetConstantBuffers(0, 4, _VSConstantBuffer);

			_context->PSSetShader(_PS, _PSClassInstances, _numPSClassInstances);
			_context->PSSetShaderResources(0, 9, _PSSRV);
			_context->PSSetSamplers(0, 10, _samplerState);
			_context->PSSetConstantBuffers(0, 3, _constantBuffers);
			_context->OMSetRenderTargets(2, _RTView, _depthView);
			
			_context->OMSetBlendState(_blendState, _blendFactor, _sampleMask);
			_context->OMSetDepthStencilState(_depthStencilState, _stencilRef);
			_context->RSSetViewports(_numViewports, _RSViewports);

			_context->GSSetShader(_GS, _GSClassInstances, _numGSClassInstances);
			_context->GSSetConstantBuffers(0, 2, _GSConstantBuffer);
			_context->GSSetShaderResources(0, 1, &_GSSRV);

			_context->HSSetShader(_HS, _HSClassInstances, _numHSClassInstances);

			_context->DSSetShader(_DS, _DSClassInstances, _numDSClassInstances);
			_context->DSSetSamplers(0, 3, _DSSamplers);

			_context->IASetVertexBuffers(0, 3, _VB, _vertexStride, _vertexOffset);
			_context->IASetIndexBuffer(_indexBuffer, _indexFormat, _indexOffset);
		}
		//---------------------------------------------------------------------------
		void Direct3D11StateBlock::Release()
		{
			#define Release_(_1)\
			if (_1 != nullptr) { _1->Release(); _1 = nullptr; }
			#define MultiRelease_(_1, count)\
			for (auto i = 0; i < count; ++i)\
				Release_(_1[i]);

			Release_(_inputLayout);

			Release_(_rasterizerState);

			Release_(_VS);
			MultiRelease_(_VSClassInstances, _numVSClassInstances);
			MultiRelease_(_VSConstantBuffer, 4);

			Release_(_PS);
			MultiRelease_(_PSClassInstances, _numPSClassInstances);
			MultiRelease_(_PSSRV, 9);
			MultiRelease_(_samplerState, 10);
			MultiRelease_(_constantBuffers, 3);

			MultiRelease_(_RTView, 2);
			Release_(_depthView);

			Release_(_blendState);
			Release_(_depthStencilState);

			Release_(_GS);
			MultiRelease_(_GSClassInstances, _numGSClassInstances);
			MultiRelease_(_GSConstantBuffer, 2);
			Release_(_GSSRV);

			Release_(_HS);
			MultiRelease_(_HSClassInstances, _numHSClassInstances);

			Release_(_DS);
			MultiRelease_(_DSClassInstances, _numDSClassInstances);
			MultiRelease_(_DSSamplers, 3);

			MultiRelease_(_VB, 3);
			Release_(_indexBuffer);
		}
		//---------------------------------------------------------------------------
	}
}
