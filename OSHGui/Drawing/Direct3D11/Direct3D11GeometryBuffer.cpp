#include "Direct3D11GeometryBuffer.hpp"
#include "Direct3D11Texture.hpp"
#include "../Vertex.hpp"
#include "../../Misc/Exceptions.hpp"
#include <algorithm>

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D11GeometryBuffer::Direct3D11GeometryBuffer(Direct3D11Renderer &_owner)
			: owner(_owner),
			  vertexBuffer(nullptr),
			  bufferSize(0),
			  bufferSynched(false),
			  clipRect(0, 0, 0, 0),
			  clippingActive(true),
			  translation(0, 0, 0),
			  rotation(0, 0, 0),
			  pivot(0, 0, 0),
			  matrixValid(false)
		{

		}
		//---------------------------------------------------------------------------
		Direct3D11GeometryBuffer::~Direct3D11GeometryBuffer()
		{
			CleanupVertexBuffer();
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		void Direct3D11GeometryBuffer::SetTranslation(const Vector &translation)
		{
			this->translation = translation;
			matrixValid = false;
		}
		//---------------------------------------------------------------------------
		void Direct3D11GeometryBuffer::SetRotation(const Quaternion &rotation)
		{
			this->rotation = rotation;
			matrixValid = false;
		}
		//---------------------------------------------------------------------------
		void Direct3D11GeometryBuffer::SetPivot(const Vector &pivot)
		{
			this->pivot = pivot;
			matrixValid = false;
		}
		//---------------------------------------------------------------------------
		void Direct3D11GeometryBuffer::SetClippingRegion(const RectangleF &region)
		{
			clipRect.SetTop(std::max(0.0f, region.GetTop()));
			clipRect.SetBottom(std::max(0.0f, region.GetBottom()));
			clipRect.SetLeft(std::max(0.0f, region.GetLeft()));
			clipRect.SetRight(std::max(0.0f, region.GetRight()));
		}
		//---------------------------------------------------------------------------
		void Direct3D11GeometryBuffer::SetActiveTexture(const TexturePtr &texture)
		{
			activeTexture = std::static_pointer_cast<Direct3D11Texture>(texture);
		}
		//---------------------------------------------------------------------------
		void Direct3D11GeometryBuffer::SetClippingActive(const bool active)
		{
			clippingActive = active;
		}
		//---------------------------------------------------------------------------
		bool Direct3D11GeometryBuffer::IsClippingActive() const
		{
			return clippingActive;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Direct3D11GeometryBuffer::AppendVertex(const Vertex &vertex)
		{
			AppendGeometry(&vertex, 1);
		}
		//---------------------------------------------------------------------------
		void Direct3D11GeometryBuffer::AppendGeometry(const Vertex *const vbuff, uint32_t count)
		{
			PerformBatchManagement();

			batches.back().count += count;

			auto vs = vbuff;
			for (auto i = 0u; i < count; ++i, ++vs)
			{
				vertices.emplace_back(vs->Position.x, vs->Position.y, vs->Position.z, vs->Color.GetARGB(), vs->TextureCoordinates.X, vs->TextureCoordinates.Y);
			}

			bufferSynched = false;
		}
		//---------------------------------------------------------------------------
		void Direct3D11GeometryBuffer::Draw() const
		{
			D3D10_RECT clip = { clipRect.GetLeft(), clipRect.GetTop(), clipRect.GetRight(), clipRect.GetBottom() };
			owner.GetDevice().Context->RSSetScissorRects(1, &clip);

			if (!bufferSynched)
			{
				SyncHardwareBuffer();
			}

			if (!matrixValid)
			{
				UpdateMatrix();
			}

			owner.SetWorldMatrix(matrix);

			UINT stride = sizeof(D3DVertex);
			UINT offset = 0;
			owner.GetDevice().Context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

			for (int pass = 0; pass < 1; ++pass)
			{
				auto pos = 0;
				for (auto &batch : batches)
				{
					owner.GetDevice().Context->IASetPrimitiveTopology(batch.mode == VertexDrawMode::TriangleList ? D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST : D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP);
					owner.SetCurrentTextureShaderResource(batch.texture);
					owner.BindTechniquePass(batch.clip);
					owner.GetDevice().Context->Draw(batch.count, pos);

					pos += batch.count;
				}
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D11GeometryBuffer::Reset()
		{
			batches.clear();
			vertices.clear();
			activeTexture = nullptr;
		}
		//---------------------------------------------------------------------------
		void Direct3D11GeometryBuffer::PerformBatchManagement()
		{
			auto texture = activeTexture ? activeTexture->GetDirect3DShaderResourceView() : nullptr;

			if (batches.empty() || texture != batches.back().texture || drawMode != batches.back().mode || clippingActive != batches.back().clip)
			{
				batches.emplace_back(texture, 0, drawMode, clippingActive);
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D11GeometryBuffer::UpdateMatrix() const
		{
			D3DXVECTOR3 p(pivot.x, pivot.y, pivot.z);
			D3DXVECTOR3 t(translation.x, translation.y, translation.z);

			D3DXQUATERNION r(rotation.x, rotation.y, rotation.z, rotation.w);

			D3DXMatrixTransformation(&matrix, nullptr, nullptr, nullptr, &p, &r, &t);

			matrixValid = true;
		}
		//---------------------------------------------------------------------------
		void Direct3D11GeometryBuffer::SyncHardwareBuffer() const
		{
			auto count = vertices.size();

			if (count > bufferSize)
			{
				CleanupVertexBuffer();
				AllocateVertexBuffer(count);
			}

			if (count > 0)
			{
				D3D11_MAPPED_SUBRESOURCE mappedSubresource;
				if (FAILED(owner.GetDevice().Context->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource)))
				{
					throw Misc::Exception();
				}

				std::memcpy(mappedSubresource.pData, vertices.data(), sizeof(D3DVertex) * count);

				owner.GetDevice().Context->Unmap(vertexBuffer, 0);
			}

			bufferSynched = true;
		}
		//---------------------------------------------------------------------------
		void Direct3D11GeometryBuffer::AllocateVertexBuffer(const uint32_t count) const
		{
			D3D11_BUFFER_DESC bufferDescription;
			bufferDescription.Usage = D3D11_USAGE_DYNAMIC;
			bufferDescription.ByteWidth = count * sizeof(D3DVertex);
			bufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bufferDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			bufferDescription.MiscFlags = 0;

			if (FAILED(owner.GetDevice().Device->CreateBuffer(&bufferDescription, 0, &vertexBuffer)))
			{
				throw Misc::Exception();
			}

			bufferSize = count;
		}
		//---------------------------------------------------------------------------
		void Direct3D11GeometryBuffer::CleanupVertexBuffer() const
		{
			if (vertexBuffer)
			{
				vertexBuffer->Release();
				vertexBuffer = nullptr;
				bufferSize = 0;
			}
		}
		//---------------------------------------------------------------------------
	}
}
