/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_OPENGLRENDERTARGET_HPP
#define OSHGUI_DRAWING_OPENGLRENDERTARGET_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "OpenGLRenderer.hpp"
#include "OpenGLGeometryBuffer.hpp"
#include "../RenderQueue.hpp"
#include "../RenderTarget.hpp"
#include "../Rectangle.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		template <typename T = RenderTarget>
		class OpenGLRenderTarget : public T
		{
		public:
			//---------------------------------------------------------------------------
			//Constructor
			//---------------------------------------------------------------------------
			OpenGLRenderTarget(OpenGLRenderer &_owner)
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
				glViewport(static_cast<GLsizei>(area.GetLeft()), static_cast<GLsizei>(area.GetTop()), static_cast<GLsizei>(area.GetWidth()), static_cast<GLsizei>(area.GetHeight()));
			
				if (!matrixValid)
				{
					UpdateMatrix();
				}

				glMatrixMode(GL_PROJECTION);
				glLoadMatrixf(glm::value_ptr(matrix));
			}
			//---------------------------------------------------------------------------
			void Deactivate() override
			{
			
			}
			//---------------------------------------------------------------------------

		protected:
			void UpdateMatrix() const
			{
				const float width = area.GetWidth();
				const float height = area.GetHeight();
				const bool widthAndHeightNotZero = (width != 0.0f) && (height != 0.0f);
				const float aspect = widthAndHeightNotZero ? width / height : 1.0f;
				const float halfWidth = widthAndHeightNotZero ? width * 0.5f : 0.5f;
				const float halfHeight = widthAndHeightNotZero ? height * 0.5f : 0.5f;
				viewDistance = halfWidth / (aspect * 0.267949192431123f);

				glm::vec3 eye(halfWidth, halfHeight, -viewDistance);
				glm::vec3 at(halfWidth, halfHeight, 1);
				glm::vec3 up(0, -1, 0);

				glm::mat4 projectionMatrix = glm::perspective(30.f, aspect, viewDistance * 0.5f, viewDistance * 2.0f);
				glm::mat4 viewMatrix = glm::lookAt(eye, at, up);

				matrix = projectionMatrix * viewMatrix;

				matrixValid = true;
			}
			//---------------------------------------------------------------------------

			OpenGLRenderer &owner;
			
			RectangleF area;
			
			mutable glm::mat4 matrix;
			mutable bool matrixValid;
			mutable float viewDistance;
		};
	}
}

#endif