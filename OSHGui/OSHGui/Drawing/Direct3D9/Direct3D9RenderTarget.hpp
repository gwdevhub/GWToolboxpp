/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_DIRECT3D9RENDERTARGET_HPP
#define OSHGUI_DRAWING_DIRECT3D9RENDERTARGET_HPP

#include "Direct3D9Renderer.hpp"
#include "Direct3D9GeometryBuffer.hpp"
#include "../RenderQueue.hpp"
#include "../RenderTarget.hpp"
#include "../Rectangle.hpp"

#include <d3dx9.h>

namespace OSHGui
{
	namespace Drawing
	{
		template <typename T = RenderTarget>
		class Direct3D9RenderTarget : public T
		{
		public:
			//---------------------------------------------------------------------------
			//Constructor
			//---------------------------------------------------------------------------
			Direct3D9RenderTarget(Direct3D9Renderer &_owner)
				: owner(_owner),
				  area(0, 0, 0, 0),
				  viewDistance(0),
				  matrixValid(false)
			{
			
			}
			//---------------------------------------------------------------------------
			//Getter/Setter
			//---------------------------------------------------------------------------
			void SetArea(const RectangleF &_area) override
			{
				area = _area;
				matrixValid = false;
			}
			//---------------------------------------------------------------------------
			const RectangleF& GetArea() const override
			{
				return area;
			}
			//---------------------------------------------------------------------------
			//Runtime-Functions
			//---------------------------------------------------------------------------
			void Draw(const GeometryBuffer &buffer) override
			{
				buffer.Draw();
			}
			//---------------------------------------------------------------------------
			void Draw(const RenderQueue &queue) override
			{
				queue.Draw();
			}
			//---------------------------------------------------------------------------
			void Activate() override
			{
				if (!matrixValid)
				{
					UpdateMatrix();
				}

				D3DVIEWPORT9 vp;
				SetupViewport(vp);
				owner.GetDevice()->SetViewport(&vp);

				owner.GetDevice()->SetTransform(D3DTS_PROJECTION, &matrix);
			}
			//---------------------------------------------------------------------------
			void Deactivate() override
			{
			
			}
			//---------------------------------------------------------------------------

		protected:
			void UpdateMatrix() const
			{
				const float fov = 0.523598776f;
				const float width = area.GetWidth();
				const float height = area.GetHeight();
				const float aspect = width / height;
				const float halfWidth = width * 0.5f;
				const float halfHeight = height * 0.5f;
				viewDistance = halfWidth / (aspect * 0.267949192431123f);

				D3DXVECTOR3 eye(halfWidth, halfHeight, -viewDistance);
				D3DXVECTOR3 at(halfWidth, halfHeight, 1);
				D3DXVECTOR3 up(0, -1, 0);

				D3DXMATRIX tmp;
				D3DXMatrixMultiply(&matrix, D3DXMatrixLookAtRH(&matrix, &eye, &at, &up), D3DXMatrixPerspectiveFovRH(&tmp, fov, aspect, viewDistance * 0.5f, viewDistance * 2.0f));

				matrixValid = true;
			}
			//---------------------------------------------------------------------------
			void SetupViewport(D3DVIEWPORT9 &viewport) const
			{
				viewport.X = area.GetLeft();
				viewport.Y = area.GetTop();
				viewport.Width = area.GetWidth();
				viewport.Height = area.GetHeight();
				viewport.MinZ = 0.0f;
				viewport.MaxZ = 1.0f;
			}
			//---------------------------------------------------------------------------

			Direct3D9Renderer &owner;
			
			RectangleF area;
			
			mutable D3DXMATRIX matrix;
			mutable bool matrixValid;
			mutable float viewDistance;
		};
	}
}

#endif