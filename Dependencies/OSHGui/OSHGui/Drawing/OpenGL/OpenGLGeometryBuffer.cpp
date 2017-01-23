#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

#include "OpenGLGeometryBuffer.hpp"
#include "OpenGLTexture.hpp"
#include "../Vertex.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		OpenGLGeometryBuffer::OpenGLGeometryBuffer(OpenGLRenderer &_owner)
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
		void OpenGLGeometryBuffer::SetTranslation(const Vector &translation)
		{
			this->translation = translation;
			matrixValid = false;
		}
		//---------------------------------------------------------------------------
		void OpenGLGeometryBuffer::SetRotation(const Quaternion &rotation)
		{
			this->rotation = rotation;
			matrixValid = false;
		}
		//---------------------------------------------------------------------------
		void OpenGLGeometryBuffer::SetPivot(const Vector &pivot)
		{
			this->pivot = pivot;
			matrixValid = false;
		}
		//---------------------------------------------------------------------------
		void OpenGLGeometryBuffer::SetClippingRegion(const RectangleF &region)
		{
			clipRect.SetTop(std::max(0.0f, region.GetTop()));
			clipRect.SetBottom(std::max(0.0f, region.GetBottom()));
			clipRect.SetLeft(std::max(0.0f, region.GetLeft()));
			clipRect.SetRight(std::max(0.0f, region.GetRight()));
		}
		//---------------------------------------------------------------------------
		void OpenGLGeometryBuffer::SetActiveTexture(const TexturePtr &texture)
		{
			activeTexture = std::static_pointer_cast<OpenGLTexture>(texture);
		}
		//---------------------------------------------------------------------------
		void OpenGLGeometryBuffer::SetClippingActive(const bool active)
		{
			clippingActive = active;
		}
		//---------------------------------------------------------------------------
		bool OpenGLGeometryBuffer::IsClippingActive() const
		{
			return clippingActive;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void OpenGLGeometryBuffer::AppendVertex(const Vertex &vertex)
		{
			AppendGeometry(&vertex, 1);
		}
		//---------------------------------------------------------------------------
		void OpenGLGeometryBuffer::AppendGeometry(const Vertex *const vbuff, uint32_t count)
		{
			PerformBatchManagement();

			batches.back().count += count;

			auto vs = vbuff;
			GLVertex vertex;
			for (auto i = 0u; i < count; ++i, ++vs)
			{
				vertex.tex[0] = vs->TextureCoordinates.X;
				vertex.tex[1] = vs->TextureCoordinates.Y;
				vertex.color[0] = vs->Color.GetRed();
				vertex.color[1] = vs->Color.GetGreen();
				vertex.color[2] = vs->Color.GetBlue();
				vertex.color[3] = vs->Color.GetAlpha();
				vertex.position[0] = vs->Position.x;
				vertex.position[1] = vs->Position.y;
				vertex.position[2] = vs->Position.z;

				vertices.emplace_back(vertex);
			}
		}
		//---------------------------------------------------------------------------
		void OpenGLGeometryBuffer::Draw() const
		{
			auto viewPort = owner.GetDisplaySize();

			/*glScissor(static_cast<GLint>(clipRect.GetLeft()),
					  static_cast<GLint>(viewPort.Height - clipRect.GetBottom()),
					  static_cast<GLint>(clipRect.GetWidth()),
					  static_cast<GLint>(clipRect.GetHeight()));*/

			if (!matrixValid)
			{
				UpdateMatrix();
			}

			glMatrixMode(GL_MODELVIEW);
			glLoadMatrixf(glm::value_ptr(matrix));

			for (int pass = 0; pass < 1; ++pass)
			{
				auto pos = 0;
				for (auto &batch : batches)
				{
					if (batch.clip)
					{
						glEnable(GL_SCISSOR_TEST);
					}
					else
					{
						glDisable(GL_SCISSOR_TEST);
					}

					glBindTexture(GL_TEXTURE_2D, batch.texture);
					glTexCoordPointer(2, GL_FLOAT, sizeof(GLVertex), &vertices[pos]);
					glColorPointer(4, GL_FLOAT, sizeof(GLVertex), &vertices[pos].color[0]);
					glVertexPointer(3, GL_FLOAT, sizeof(GLVertex), &vertices[pos].position[0]);
					
					if (batch.mode == VertexDrawMode::TriangleList)
					{
						glDrawArrays(GL_TRIANGLES, 0, batch.count);
					}
					else
					{
						glDrawArrays(GL_LINES, 0, batch.count);
					}
					
					pos += batch.count;
				}
			}
		}
		//---------------------------------------------------------------------------
		void OpenGLGeometryBuffer::Reset()
		{
			batches.clear();
			vertices.clear();
			activeTexture = nullptr;
		}
		//---------------------------------------------------------------------------
		void OpenGLGeometryBuffer::PerformBatchManagement()
		{
			auto texture = activeTexture ? activeTexture->GetOpenGLTexture() : 0;

			if (batches.empty() || texture != batches.back().texture || drawMode != batches.back().mode || clippingActive != batches.back().clip)
			{
				batches.emplace_back(texture, 0, drawMode, clippingActive);
			}
		}
		//---------------------------------------------------------------------------
		void OpenGLGeometryBuffer::UpdateMatrix() const
		{
			matrix = glm::mat4(1.f);

			const glm::vec3 final_trans(translation.x + pivot.x,
										translation.y + pivot.y,
										translation.z + pivot.z);

			matrix = glm::translate(matrix, final_trans);

			glm::quat rotationQuat = glm::quat(rotation.w, rotation.x, rotation.y, rotation.z);
			glm::mat4 rotation_matrix = glm::mat4_cast(rotationQuat);

			matrix = matrix * rotation_matrix;

			glm::vec3 transl = glm::vec3(-pivot.x, -pivot.y, -pivot.z);
			glm::mat4 translMatrix = glm::translate(glm::mat4(1.f), transl);
			matrix =  matrix * translMatrix;

			matrixValid = true;
		}
		//---------------------------------------------------------------------------
	}
}
