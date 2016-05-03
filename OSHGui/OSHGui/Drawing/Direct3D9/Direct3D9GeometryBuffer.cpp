#include "Direct3D9GeometryBuffer.hpp"
#include "Direct3D9Texture.hpp"
#include "../Vertex.hpp"
#include <algorithm>

namespace OSHGui {
	namespace Drawing {
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Direct3D9GeometryBuffer::Direct3D9GeometryBuffer(Direct3D9Renderer &_owner)
			: owner(_owner),
			  clipRect(0, 0, 0, 0),
			  clippingActive(true),
			  translation(0, 0),
			  rotation(0, 0, 0),
			  pivot(0, 0),
			  matrixValid(false) {

		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		void Direct3D9GeometryBuffer::SetTranslation(const PointF &translation) {
			this->translation = translation;
			matrixValid = false;
		}
		//---------------------------------------------------------------------------
		void Direct3D9GeometryBuffer::SetRotation(const Quaternion &rotation) {
			this->rotation = rotation;
			matrixValid = false;
		}
		//---------------------------------------------------------------------------
		void Direct3D9GeometryBuffer::SetPivot(const PointF &pivot) {
			this->pivot = pivot;
			matrixValid = false;
		}
		//---------------------------------------------------------------------------
		void Direct3D9GeometryBuffer::SetClippingRegion(const RectangleI &region) {
			clipRect.SetTop((std::max)(0, region.GetTop()));
			clipRect.SetBottom((std::max)(0, region.GetBottom()));
			clipRect.SetLeft((std::max)(0, region.GetLeft()));
			clipRect.SetRight((std::max)(0, region.GetRight()));
		}
		RectangleI Direct3D9GeometryBuffer::GetClippingRegion() {
			return clipRect;
		}
		//---------------------------------------------------------------------------
		void Direct3D9GeometryBuffer::SetActiveTexture(const TexturePtr &texture) {
			activeTexture = std::static_pointer_cast<Direct3D9Texture>(texture);
		}
		//---------------------------------------------------------------------------
		void Direct3D9GeometryBuffer::SetClippingActive(const bool active) {
			clippingActive = active;
		}
		//---------------------------------------------------------------------------
		bool Direct3D9GeometryBuffer::IsClippingActive() const {
			return clippingActive;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void Direct3D9GeometryBuffer::AppendVertex(const Vertex &vertex) {
			AppendGeometry(&vertex, 1);
		}
		//---------------------------------------------------------------------------
		void Direct3D9GeometryBuffer::AppendGeometry(const Vertex *const vbuff, uint32_t count) {
			PerformBatchManagement();

			batches.back().count += count;

			const Drawing::Vertex* vs = vbuff;
			for (auto i = 0u; i < count; ++i, ++vs) {
				vertices.emplace_back(vs->Position.X - 0.5f, vs->Position.Y -.5f, 0.0f, vs->Color.GetARGB(), vs->TextureCoordinates.X, vs->TextureCoordinates.Y);
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D9GeometryBuffer::Draw() const {
			auto device = owner.GetDevice();

			RECT clipBackup;
			device->GetScissorRect(&clipBackup);

			if (!matrixValid) {
				UpdateMatrix();
			}

			device->SetTransform(D3DTS_WORLD, &matrix);

			RECT clip = { clipRect.GetLeft(), clipRect.GetTop(), clipRect.GetRight(), clipRect.GetBottom() };
			//RECT clip = { 0, 200, 1900, 800 };
			for (int pass = 0; pass < 1; ++pass) {
				uint32_t pos = 0;
				for (const Drawing::Direct3D9GeometryBuffer::BatchInfo &batch : batches) {
					//if (batch.clip) {
						device->SetScissorRect(&clip);
					//}

					device->SetTexture(0, batch.texture);
					if (batch.mode == VertexDrawMode::TriangleList) {
						device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, batch.count / 3, &vertices[pos], sizeof(D3DVertex));
					} else {
						device->DrawPrimitiveUP(D3DPT_LINELIST, batch.count / 2, &vertices[pos], sizeof(D3DVertex));
					}
					pos += batch.count;

					//if (batch.clip) {
						device->SetScissorRect(&clipBackup);
					//}
				}
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D9GeometryBuffer::Reset() {
			batches.clear();
			vertices.clear();
			activeTexture = nullptr;
		}
		//---------------------------------------------------------------------------
		void Direct3D9GeometryBuffer::PerformBatchManagement() {
			IDirect3DTexture9* texture = activeTexture ? activeTexture->GetDirect3D9Texture() : nullptr;

			if (batches.empty() || texture != batches.back().texture || drawMode != batches.back().mode || clippingActive != batches.back().clip) {
				batches.emplace_back(texture, 0, drawMode, clippingActive);
			}
		}
		//---------------------------------------------------------------------------
		void Direct3D9GeometryBuffer::UpdateMatrix() const {
			D3DXVECTOR3 p(pivot.X, pivot.Y, 0);
			D3DXVECTOR3 t(translation.X, translation.Y, 0);

			D3DXQUATERNION r(rotation.x, rotation.y, rotation.z, rotation.w);

			D3DXMatrixTransformation(&matrix, nullptr, nullptr, nullptr, &p, &r, &t);

			matrixValid = true;
		}
		//---------------------------------------------------------------------------
	}
}
