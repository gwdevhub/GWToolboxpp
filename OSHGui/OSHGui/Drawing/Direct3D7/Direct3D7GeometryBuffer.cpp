#include "Direct3D7GeometryBuffer.hpp"
#include "Direct3D7Texture.hpp"
#include "../Vertex.hpp"
#include <algorithm>

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D7GeometryBuffer::Direct3D7GeometryBuffer(Direct3D7Renderer &_owner)
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
		void Direct3D7GeometryBuffer::SetTranslation(const Vector &translation)
		{
			this->translation = translation;
			matrixValid = false;
		}
		//---------------------------------------------------------------------------
		void Direct3D7GeometryBuffer::SetRotation(const Quaternion &rotation)
		{
			this->rotation = rotation;
			matrixValid = false;
		}
		//---------------------------------------------------------------------------
		void Direct3D7GeometryBuffer::SetPivot(const Vector &pivot)
		{
			this->pivot = pivot;
			matrixValid = false;
		}
		//---------------------------------------------------------------------------
		void Direct3D7GeometryBuffer::SetClippingRegion(const RectangleF &region)
		{
			clipRect.SetTop(std::max(0.0f, region.GetTop()));
			clipRect.SetBottom(std::max(0.0f, region.GetBottom()));
			clipRect.SetLeft(std::max(0.0f, region.GetLeft()));
			clipRect.SetRight(std::max(0.0f, region.GetRight()));
		}
		//---------------------------------------------------------------------------
		void Direct3D7GeometryBuffer::SetActiveTexture(const TexturePtr &texture)
		{
			activeTexture = std::static_pointer_cast<Direct3D7Texture>(texture);
		}
		//---------------------------------------------------------------------------
		void Direct3D7GeometryBuffer::SetClippingActive(const bool active)
		{
			clippingActive = active;
		}
		//---------------------------------------------------------------------------
		bool Direct3D7GeometryBuffer::IsClippingActive() const
		{
			return clippingActive;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Direct3D7GeometryBuffer::Draw() const
		{
			auto device = owner.GetDevice().Device;

			D3DVIEWPORT7 viewport;
			D3DMATRIX projection;
			auto EnableClipping = [this, device, &viewport, &projection]()
			{
				device->GetViewport(&viewport);

				device->GetTransform(D3DTRANSFORMSTATE_PROJECTION, &projection);
				auto csz(clipRect.GetSize());
				SizeF tsz(viewport.dwWidth, viewport.dwHeight);

				float m_00 = tsz.Width / csz.Width;
				float m_11 = tsz.Height / csz.Height;
				float m_30 = 1.0f * (tsz.Width + 2.0f * (viewport.dwX - (clipRect.GetLeft() + csz.Width * 0.5f))) / csz.Width;
				float m_31 = -(tsz.Height + 2.0f * (viewport.dwY - (clipRect.GetTop() + csz.Height * 0.5f))) / csz.Height;

				D3DMATRIX scissor(
					m_00, 0.0f, 0.0f, 0.0f,
					0.0f, m_11, 0.0f, 0.0f,
					0.0f, 0.0f, 1.0f, 0.0f,
					m_30, m_31, 0.0f, 1.0f
				);

				D3DMATRIX scissorProjection;
				MatrixMultiply(&scissorProjection, &scissor, &projection);
				device->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &scissorProjection);

				D3DVIEWPORT7 vp = { clipRect.GetLeft(), clipRect.GetTop(), clipRect.GetWidth(), clipRect.GetHeight(), 0, 1 };
				device->SetViewport(&vp);
			};

			auto DisableClipping = [device, &viewport, &projection]()
			{
				device->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &projection);
				device->SetViewport(&viewport);
			};

			if (!matrixValid)
			{
				UpdateMatrix();
			}

			device->SetTransform(D3DTRANSFORMSTATE_WORLD, &matrix);

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
						device->DrawPrimitive(D3DPT_TRIANGLELIST, D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1, (LPVOID)&vertices[pos], batch.count, 0);
					}
					else
					{
						device->DrawPrimitive(D3DPT_LINELIST, D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1, (LPVOID)&vertices[pos], batch.count, 0);
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
		void Direct3D7GeometryBuffer::AppendVertex(const Vertex &vertex)
		{
			AppendGeometry(&vertex, 1);
		}
		//---------------------------------------------------------------------------
		void Direct3D7GeometryBuffer::AppendGeometry(const Vertex *const vbuff, uint32_t count)
		{
			PerformBatchManagement();

			batches.back().count += count;

			auto vs = vbuff;
			for (auto i = 0; i < count; ++i, ++vs)
			{
				vertices.emplace_back(vs->Position.x - 0.5f, vs->Position.y - 0.5f, vs->Position.z, vs->Color.GetARGB(), vs->TextureCoordinates.X, vs->TextureCoordinates.Y);
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D7GeometryBuffer::Reset()
		{
			batches.clear();
			vertices.clear();
			activeTexture = nullptr;
		}
		//---------------------------------------------------------------------------
		void Direct3D7GeometryBuffer::PerformBatchManagement()
		{
			auto texture = activeTexture ? activeTexture->GetDirect3D7Texture() : nullptr;

			if (batches.empty() || texture != batches.back().texture || drawMode != batches.back().mode || clippingActive != batches.back().clip)
			{
				batches.emplace_back(texture, 0, drawMode, clippingActive);
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D7GeometryBuffer::UpdateMatrix() const
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
