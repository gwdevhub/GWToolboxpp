#include "Direct3D8GeometryBuffer.hpp"
#include "Direct3D8Texture.hpp"
#include "../Vertex.hpp"
#include <algorithm>

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D8GeometryBuffer::Direct3D8GeometryBuffer(Direct3D8Renderer &_owner)
			: owner(_owner),
			  clipRect(0, 0, 0, 0),
			  clippingActive(true),
			  translation(0, 0, 0),
			  rotation(0, 0, 0),
			  pivot(0, 0, 0),
			  matrixValid(false)
		{

		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		void Direct3D8GeometryBuffer::SetTranslation(const Vector &translation)
		{
			this->translation = translation;
			matrixValid = false;
		}
		//---------------------------------------------------------------------------
		void Direct3D8GeometryBuffer::SetRotation(const Quaternion &rotation)
		{
			this->rotation = rotation;
			matrixValid = false;
		}
		//---------------------------------------------------------------------------
		void Direct3D8GeometryBuffer::SetPivot(const Vector &pivot)
		{
			this->pivot = pivot;
			matrixValid = false;
		}
		//---------------------------------------------------------------------------
		void Direct3D8GeometryBuffer::SetClippingRegion(const RectangleF &region)
		{
			clipRect.SetTop(std::max(0.0f, region.GetTop()));
			clipRect.SetBottom(std::max(0.0f, region.GetBottom()));
			clipRect.SetLeft(std::max(0.0f, region.GetLeft()));
			clipRect.SetRight(std::max(0.0f, region.GetRight()));
		}
		//---------------------------------------------------------------------------
		void Direct3D8GeometryBuffer::SetActiveTexture(const TexturePtr &texture)
		{
			activeTexture = std::static_pointer_cast<Direct3D8Texture>(texture);
		}
		//---------------------------------------------------------------------------
		void Direct3D8GeometryBuffer::SetClippingActive(const bool active)
		{
			clippingActive = active;
		}
		//---------------------------------------------------------------------------
		bool Direct3D8GeometryBuffer::IsClippingActive() const
		{
			return clippingActive;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Direct3D8GeometryBuffer::Draw() const
		{
			auto device = owner.GetDevice();

			D3DVIEWPORT8 viewport;
			D3DXMATRIX projection;
			auto EnableClipping = [this, device, &viewport, &projection]()
			{
				device->GetViewport(&viewport);

				device->GetTransform(D3DTS_PROJECTION, &projection);
				auto csz(clipRect.GetSize());
				SizeF tsz(static_cast<float>(viewport.Width), static_cast<float>(viewport.Height));

				float m_00 = tsz.Width / csz.Width;
				float m_11 = tsz.Height / csz.Height;
				float m_30 = 1.0f * (tsz.Width + 2.0f * (viewport.X - (clipRect.GetLeft() + csz.Width * 0.5f))) / csz.Width;
				float m_31 = -(tsz.Height + 2.0f * (viewport.Y - (clipRect.GetTop() + csz.Height * 0.5f))) / csz.Height;

				D3DXMATRIX scissor(
					m_00, 0.0f, 0.0f, 0.0f,
					0.0f, m_11, 0.0f, 0.0f,
					0.0f, 0.0f, 1.0f, 0.0f,
					m_30, m_31, 0.0f, 1.0f
				);

				D3DXMATRIX scissorProjection;
				MatrixMultiply(&scissorProjection, &scissor, &projection);
				device->SetTransform(D3DTS_PROJECTION, &scissorProjection);

				D3DVIEWPORT8 vp = { clipRect.GetLeft(), clipRect.GetTop(), clipRect.GetWidth(), clipRect.GetHeight(), 0, 1 };
				device->SetViewport(&vp);
			};

			auto DisableClipping = [device, &viewport, &projection]()
			{
				device->SetTransform(D3DTS_PROJECTION, &projection);
				device->SetViewport(&viewport);
			};

			if (!matrixValid)
			{
				UpdateMatrix();
			}

			device->SetTransform(D3DTS_WORLD, &matrix);

			for (int pass = 0; pass < 1; ++pass)
			{
				auto pos = 0;
				for (auto &batch : batches)
				{
					if (batch.clip)
					{
						EnableClipping();
					}

					device->SetTexture(0, batch.texture);
					if (batch.mode == VertexDrawMode::TriangleList)
					{
						device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, batch.count / 3, &vertices[pos], sizeof(D3DVertex));
					}
					else
					{
						device->DrawPrimitiveUP(D3DPT_LINELIST, batch.count / 2, &vertices[pos], sizeof(D3DVertex));
					}

					if (batch.clip)
					{
						DisableClipping();
					}

					pos += batch.count;
				}
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D8GeometryBuffer::AppendVertex(const Vertex &vertex)
		{
			AppendGeometry(&vertex, 1);
		}
		//---------------------------------------------------------------------------
		void Direct3D8GeometryBuffer::AppendGeometry(const Vertex *const vbuff, uint32_t count)
		{
			PerformBatchManagement();

			batches.back().count += count;

			auto vs = vbuff;
			for (auto i = 0u; i < count; ++i, ++vs)
			{
				vertices.emplace_back(vs->Position.x - 0.5f, vs->Position.y - 0.5f, vs->Position.z, vs->Color.GetARGB(), vs->TextureCoordinates.X, vs->TextureCoordinates.Y);
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D8GeometryBuffer::Reset()
		{
			batches.clear();
			vertices.clear();
			activeTexture = nullptr;
		}
		//---------------------------------------------------------------------------
		void Direct3D8GeometryBuffer::PerformBatchManagement()
		{
			auto texture = activeTexture ? activeTexture->GetDirect3D8Texture() : nullptr;

			if (batches.empty() || texture != batches.back().texture || drawMode != batches.back().mode || clippingActive != batches.back().clip)
			{
				batches.emplace_back(texture, 0, drawMode, clippingActive);
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D8GeometryBuffer::UpdateMatrix() const
		{
			D3DXVECTOR3 p(pivot.x, pivot.y, pivot.z);
			D3DXVECTOR3 t(translation.x, translation.y, translation.z);

			D3DXQUATERNION r(rotation.x, rotation.y, rotation.z, rotation.w);

			MatrixTransformation(&matrix, nullptr, nullptr, nullptr, &p, &r, &t);

			matrixValid = true;
		}
		//---------------------------------------------------------------------------
	}
}
