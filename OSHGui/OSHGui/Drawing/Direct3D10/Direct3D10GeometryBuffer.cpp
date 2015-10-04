#include "Direct3D10GeometryBuffer.hpp"
#include "Direct3D10Texture.hpp"
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
		Direct3D10GeometryBuffer::Direct3D10GeometryBuffer(Direct3D10Renderer &_owner)
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
		Direct3D10GeometryBuffer::~Direct3D10GeometryBuffer()
		{
			CleanupVertexBuffer();
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		void Direct3D10GeometryBuffer::SetTranslation(const Vector &translation)
		{
			this->translation = translation;
			matrixValid = false;
		}
		//---------------------------------------------------------------------------
		void Direct3D10GeometryBuffer::SetRotation(const Quaternion &rotation)
		{
			this->rotation = rotation;
			matrixValid = false;
		}
		//---------------------------------------------------------------------------
		void Direct3D10GeometryBuffer::SetPivot(const Vector &pivot)
		{
			this->pivot = pivot;
			matrixValid = false;
		}
		//---------------------------------------------------------------------------
		void Direct3D10GeometryBuffer::SetClippingRegion(const RectangleF &region)
		{
			clipRect.SetTop(std::max(0.0f, region.GetTop()));
			clipRect.SetBottom(std::max(0.0f, region.GetBottom()));
			clipRect.SetLeft(std::max(0.0f, region.GetLeft()));
			clipRect.SetRight(std::max(0.0f, region.GetRight()));
		}
		//---------------------------------------------------------------------------
		void Direct3D10GeometryBuffer::SetActiveTexture(const TexturePtr &texture)
		{
			activeTexture = std::static_pointer_cast<Direct3D10Texture>(texture);
		}
		//---------------------------------------------------------------------------
		void Direct3D10GeometryBuffer::SetClippingActive(const bool active)
		{
			clippingActive = active;
		}
		//---------------------------------------------------------------------------
		bool Direct3D10GeometryBuffer::IsClippingActive() const
		{
			return clippingActive;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Direct3D10GeometryBuffer::AppendVertex(const Vertex &vertex)
		{
			AppendGeometry(&vertex, 1);
		}
		//---------------------------------------------------------------------------
		void Direct3D10GeometryBuffer::AppendGeometry(const Vertex *const vbuff, uint32_t count)
		{
			PerformBatchManagement();

			batches.back().count += count;

			auto vs = vbuff;
			for (auto i = 0; i < count; ++i, ++vs)
			{
				vertices.emplace_back(vs->Position.x, vs->Position.y, vs->Position.z, vs->Color.GetARGB(), vs->TextureCoordinates.X, vs->TextureCoordinates.Y);
			}

			bufferSynched = false;
		}
		//---------------------------------------------------------------------------
		void Direct3D10GeometryBuffer::Draw() const
		{
			D3D10_RECT clip = { clipRect.GetLeft(), clipRect.GetTop(), clipRect.GetRight(), clipRect.GetBottom() };
			owner.GetDevice()->RSSetScissorRects(1, &clip);

			if (!bufferSynched)
			{
				SyncHardwareBuffer();
			}

			if (!matrixValid)
			{
				UpdateMatrix();
			}

			owner.SetWorldMatrix(matrix);

			auto stride = sizeof(D3DVertex);
			uint32_t offset = 0;
			owner.GetDevice()->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

			for (int pass = 0; pass < 1; ++pass)
			{
				auto pos = 0;
				for (auto &batch : batches)
				{
					owner.GetDevice()->IASetPrimitiveTopology(batch.mode == VertexDrawMode::TriangleList ? D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST : D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP);
					owner.SetCurrentTextureShaderResource(batch.texture);
					owner.BindTechniquePass(batch.clip);
					owner.GetDevice()->Draw(batch.count, pos);

					pos += batch.count;
				}
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D10GeometryBuffer::Reset()
		{
			batches.clear();
			vertices.clear();
			activeTexture = nullptr;
		}
		//---------------------------------------------------------------------------
		void Direct3D10GeometryBuffer::PerformBatchManagement()
		{
			auto texture = activeTexture ? activeTexture->GetDirect3DShaderResourceView() : nullptr;

			if (batches.empty() || texture != batches.back().texture || drawMode != batches.back().mode || clippingActive != batches.back().clip)
			{
				batches.emplace_back(texture, 0, drawMode, clippingActive);
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D10GeometryBuffer::UpdateMatrix() const
		{
			D3DXVECTOR3 p(pivot.x, pivot.y, pivot.z);
			D3DXVECTOR3 t(translation.x, translation.y, translation.z);

			D3DXQUATERNION r(rotation.x, rotation.y, rotation.z, rotation.w);

			D3DXMatrixTransformation(&matrix, nullptr, nullptr, nullptr, &p, &r, &t);

			matrixValid = true;
		}
		//---------------------------------------------------------------------------
		void Direct3D10GeometryBuffer::SyncHardwareBuffer() const
		{
			auto count = vertices.size();

			if (count > bufferSize)
			{
				CleanupVertexBuffer();
				AllocateVertexBuffer(count);
			}

			if (count > 0)
			{
				void *buffer;
				if (FAILED(vertexBuffer->Map(D3D10_MAP_WRITE_DISCARD, 0, &buffer)))
				{
					throw Misc::Exception();
				}

				std::memcpy(buffer, vertices.data(), sizeof(D3DVertex) * count);

				vertexBuffer->Unmap();
			}

			bufferSynched = true;
		}
		//---------------------------------------------------------------------------
		void Direct3D10GeometryBuffer::AllocateVertexBuffer(const uint32_t count) const
		{
			D3D10_BUFFER_DESC bufferDescription;
			bufferDescription.Usage = D3D10_USAGE_DYNAMIC;
			bufferDescription.ByteWidth = count * sizeof(D3DVertex);
			bufferDescription.BindFlags = D3D10_BIND_VERTEX_BUFFER;
			bufferDescription.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
			bufferDescription.MiscFlags = 0;

			if (FAILED(owner.GetDevice()->CreateBuffer(&bufferDescription, 0, &vertexBuffer)))
			{
				throw Misc::Exception();
			}

			bufferSize = count;
		}
		//---------------------------------------------------------------------------
		void Direct3D10GeometryBuffer::CleanupVertexBuffer() const
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
