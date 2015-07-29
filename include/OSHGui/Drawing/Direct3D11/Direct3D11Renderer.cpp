#include "Direct3D11Renderer.hpp"
#include "Direct3D11Texture.hpp"
#include "Direct3D11GeometryBuffer.hpp"
#include "Direct3D11RenderTarget.hpp"
#include "Direct3D11ViewportTarget.hpp"
#include "Direct3D11TextureTarget.hpp"
#include "../../Misc/Exceptions.hpp"

#include <d3dx11effect.h>

const char shaderSource[] =
	"matrix WorldMatrix;\n"
	"matrix ProjectionMatrix;\n"
	"Texture2D Texture;\n"
	"bool UseTexture;\n"
	"\n"
	"struct VSSceneIn\n"
	"{\n"
	"	float3 pos : POSITION;\n"
	"	float4 colour : COLOR;\n"
	"	float2 tex : TEXCOORD;\n"
	"};\n"
	"\n"
	"struct PSSceneIn\n"
	"{\n"
	"	float4 pos : SV_Position;\n"
	"	float4 colour : COLOR;\n"
	"	float2 tex : TEXCOORD;\n"
	"};\n"
	"DepthStencilState DisableDepth\n"
	"{\n"
	"	DepthEnable = FALSE;\n"
	"	DepthWriteMask = ZERO;\n"
	"};\n"
	"BlendState Normal\n"
	"{\n"
	"	AlphaToCoverageEnable = false;\n"
	"	BlendEnable[0] = true;\n"
	"	SrcBlendAlpha = INV_DEST_ALPHA;\n"
	"	DestBlendAlpha = ONE;\n"
	"	SrcBlend = SRC_ALPHA;\n"
	"	DestBlend = INV_SRC_ALPHA;\n"
	"};\n"
	"SamplerState LinearSampler\n"
	"{\n"
	"	Filter = MIN_MAG_MIP_LINEAR;\n"
	"	AddressU = Clamp;\n"
	"	AddressV = Clamp;\n"
	"};\n"
	"RasterizerState ClipRasterstate\n"
	"{\n"
	"	DepthClipEnable = false;\n"
	"	FillMode = Solid;\n"
	"	CullMode = None;\n"
	"	ScissorEnable = true;\n"
	"};\n"
	"RasterizerState NoclipRasterstate\n"
	"{\n"
	"	DepthClipEnable = false;\n"
	"	FillMode = Solid;\n"
	"	CullMode = None;\n"
	"	ScissorEnable = false;\n"
	"};\n"
	"// Vertex shader\n"
	"PSSceneIn VS(VSSceneIn input)\n"
	"{\n"
	"	PSSceneIn output;\n"
	"	output.pos = mul(float4(input.pos, 1), WorldMatrix);\n"
	"	output.pos = mul(output.pos, ProjectionMatrix);\n"
	"	output.tex = input.tex;\n"
	"	output.colour.rgba = input.colour.bgra;\n"
	"	return output;\n"
	"}\n"
	"// Pixel shader\n"
	"float4 PS(PSSceneIn input) : SV_Target\n"
	"{\n"
	"	float4 colour;\n"
	"	if (UseTexture)\n"
	"		colour = Texture.Sample(LinearSampler, input.tex) * input.colour;\n"
	"	else\n"
	"		colour = input.colour;\n"
	"	return colour;\n"
	"}\n"
	"// Techniques\n"
	"technique10 ClippedRendering\n"
	"{\n"
	"	pass P0\n"
	"	{\n"
	"		SetVertexShader(CompileShader(vs_4_0, VS()));\n"
	"		SetGeometryShader(NULL);\n"
	"		SetPixelShader(CompileShader(ps_4_0, PS()));\n"
	"		SetDepthStencilState(DisableDepth, 0);\n"
	"		SetBlendState(Normal, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);\n"
	"		SetRasterizerState(ClipRasterstate);\n"
	"	}\n"
	"}\n"
	"technique10 UnclippedRendering\n"
	"{\n"
	"	pass P0\n"
	"	{\n"
	"		SetVertexShader(CompileShader(vs_4_0, VS()));\n"
	"		SetGeometryShader(NULL);\n"
	"		SetPixelShader(CompileShader(ps_4_0, PS()));\n"
	"		SetDepthStencilState(DisableDepth, 0);\n"
	"		SetBlendState(Normal, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);\n"
	"		SetRasterizerState(NoclipRasterstate);\n"
	"	}\n"
	"}\n";

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D11Renderer::Direct3D11Renderer(ID3D11Device *_device, ID3D11DeviceContext *_context)
			: device(_device, _context),
			  displaySize(GetViewportSize()),
			  displayDPI(96, 96),
			  clippedTechnique(nullptr),
			  unclippedTechnique(nullptr),
			  inputLayout(nullptr),
			  textureVariable(nullptr),
			  worldMatrixVariable(nullptr),
			  projectionMatrixVariable(nullptr),
			  useTextureVariable(nullptr),
			  defaultTarget(std::make_shared<Direct3D11ViewportTarget>(*this)),
			  stateBlock(_context)
		{
			ID3D10Blob *errors = nullptr;
			ID3D10Blob *blob = nullptr;
			if (FAILED(D3DX11CompileFromMemory(shaderSource, sizeof(shaderSource), nullptr, nullptr, nullptr, nullptr, "fx_5_0", NULL, NULL, nullptr, &blob, &errors, nullptr)))
			{
				std::string msg(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
				errors->Release();
				
				throw Misc::Exception(msg);
			}

			if (FAILED(D3DX11CreateEffectFromMemory(blob->GetBufferPointer(), blob->GetBufferSize(), 0, device.Device, &effect)))
			{
				throw Misc::Exception();
			}

			if (blob)
			{
				blob->Release();
			}

			clippedTechnique = effect->GetTechniqueByName("ClippedRendering");
			unclippedTechnique = effect->GetTechniqueByName("UnclippedRendering");

			worldMatrixVariable = effect->GetVariableByName("WorldMatrix")->AsMatrix();
			projectionMatrixVariable = effect->GetVariableByName("ProjectionMatrix")->AsMatrix();
			textureVariable = effect->GetVariableByName("Texture")->AsShaderResource();
			useTextureVariable = effect->GetVariableByName("UseTexture")->AsScalar();

			const D3D11_INPUT_ELEMENT_DESC vertexLayout[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,	  0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};

			D3DX11_PASS_DESC passDescription;
			if (FAILED(clippedTechnique->GetPassByIndex(0)->GetDesc(&passDescription)))
			{
				throw Misc::Exception();
			}

			auto count = sizeof(vertexLayout) / sizeof(vertexLayout[0]);
			if (FAILED(device.Device->CreateInputLayout(vertexLayout, count, passDescription.pIAInputSignature, passDescription.IAInputSignatureSize, &inputLayout)))
			{
				throw Misc::Exception();
			}
		}
		//---------------------------------------------------------------------------
		Direct3D11Renderer::~Direct3D11Renderer()
		{
			if (effect)
			{
				effect->Release();
			}

			if (inputLayout)
			{
				inputLayout->Release();
			}
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		RenderTargetPtr& Direct3D11Renderer::GetDefaultRenderTarget()
		{
			return defaultTarget;
		}
		//---------------------------------------------------------------------------
		void Direct3D11Renderer::SetDisplaySize(const SizeF &size)
		{
			if (size != displaySize)
			{
				displaySize = size;

				auto area = defaultTarget->GetArea();
				area.SetSize(size);
				defaultTarget->SetArea(area);
			}
		}
		//---------------------------------------------------------------------------
		const SizeF& Direct3D11Renderer::GetDisplaySize() const
		{
			return displaySize;
		}
		//---------------------------------------------------------------------------
		const PointF& Direct3D11Renderer::GetDisplayDPI() const
		{
			return displayDPI;
		}
		//---------------------------------------------------------------------------
		uint32_t Direct3D11Renderer::GetMaximumTextureSize() const
		{
			return 8192;
		}
		//---------------------------------------------------------------------------
		SizeF Direct3D11Renderer::GetViewportSize()
		{
			D3D11_VIEWPORT vp;
			uint32_t count = 1;

			device.Context->RSGetViewports(&count, &vp);

			if (count != 1)
			{
				throw Misc::Exception();
			}

			return SizeF(vp.Width, vp.Height);
		}
		//---------------------------------------------------------------------------
		Direct3D11Renderer::IDevice11& Direct3D11Renderer::GetDevice()
		{
			return device;
		}
		//---------------------------------------------------------------------------
		void Direct3D11Renderer::BindTechniquePass(const bool clipped)
		{
			if (clipped)
			{
				clippedTechnique->GetPassByIndex(0)->Apply(0, device.Context);
			}
			else
			{
				unclippedTechnique->GetPassByIndex(0)->Apply(0, device.Context);
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D11Renderer::SetCurrentTextureShaderResource(ID3D11ShaderResourceView *srv)
		{
			textureVariable->SetResource(srv);
			useTextureVariable->SetBool(srv != nullptr);
		}
		//---------------------------------------------------------------------------
		void Direct3D11Renderer::SetProjectionMatrix(D3DXMATRIX &matrix)
		{
			projectionMatrixVariable->SetMatrix(reinterpret_cast<float*>(&matrix));
		}
		//---------------------------------------------------------------------------
		void Direct3D11Renderer::SetWorldMatrix(D3DXMATRIX& matrix)
		{
			worldMatrixVariable->SetMatrix(reinterpret_cast<float*>(&matrix));
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		GeometryBufferPtr Direct3D11Renderer::CreateGeometryBuffer()
		{
			return std::make_shared<Direct3D11GeometryBuffer>(*this);
		}
		//---------------------------------------------------------------------------
		TextureTargetPtr Direct3D11Renderer::CreateTextureTarget()
		{
			return std::make_shared<Direct3D11TextureTarget>(*this);
		}
		//---------------------------------------------------------------------------
		TexturePtr Direct3D11Renderer::CreateTexture()
		{
			return std::shared_ptr<Direct3D11Texture>(new Direct3D11Texture(device));
		}
		//---------------------------------------------------------------------------
		TexturePtr Direct3D11Renderer::CreateTexture(const Misc::AnsiString &filename)
		{
			return std::shared_ptr<Direct3D11Texture>(new Direct3D11Texture(device, filename));
		}
		//---------------------------------------------------------------------------
		TexturePtr Direct3D11Renderer::CreateTexture(const SizeF &size)
		{
			return std::shared_ptr<Direct3D11Texture>(new Direct3D11Texture(device, size));
		}
		//---------------------------------------------------------------------------
		void Direct3D11Renderer::BeginRendering()
		{
			stateBlock.Capture();

			device.Context->IASetInputLayout(inputLayout);
			device.Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		}
		//---------------------------------------------------------------------------
		void Direct3D11Renderer::EndRendering()
		{
			stateBlock.Apply();
		}
		//---------------------------------------------------------------------------
	}
}