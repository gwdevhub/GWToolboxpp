/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_OPENGLGEOMETRYBUFFER_HPP
#define OSHGUI_DRAWING_OPENGLGEOMETRYBUFFER_HPP

#include "../GeometryBuffer.hpp"
#include "OpenGLRenderer.hpp"
#include "GL.hpp"
#include <vector>

namespace OSHGui
{
	namespace Drawing
	{
		class OpenGLTexture;

		/**
		 * Die OpenGL Variante des GeometryBuffer.
		 */
		class OSHGUI_EXPORT OpenGLGeometryBuffer : public GeometryBuffer
		{
		public:
			/**
			 * Konstruktor der Klasse.
			 *
			 * \param owner Renderer Objekt, das den GeometryBuffer erstellt hat
			 */
			OpenGLGeometryBuffer(OpenGLRenderer &owner);

			virtual void SetTranslation(const Vector &translation) override;
			virtual void SetRotation(const Quaternion &rotation) override;
			virtual void SetPivot(const Vector &pivot) override;
			virtual void SetClippingRegion(const RectangleF &region) override;
			virtual void SetActiveTexture(const TexturePtr &texture) override;
			virtual void SetClippingActive(const bool active) override;
			virtual bool IsClippingActive() const override;
			virtual void AppendVertex(const Vertex &vertex) override;
			virtual void AppendGeometry(const Vertex *const vertices, uint32_t count) override;
			virtual void Draw() const override;
			virtual void Reset() override;

		protected:
			/**
			 * Erledigt das Batch Management bevor neue Geometrie hinzugefügt wird.
			 */
			void PerformBatchManagement();
			/**
			 * Updatet die Transformationsmatrix vor dem Zeichnen.
			 */
			void UpdateMatrix() const;

			struct GLVertex
			{
				float tex[2];
				float color[4];
				float position[3];
			};

			struct BatchInfo
			{
				BatchInfo(GLuint _texture, uint32_t _count, VertexDrawMode _mode, bool _clip)
					: texture(_texture),
					  count(_count),
					  mode(_mode),
					  clip(_clip)
				{

				}

				GLuint texture;
				uint32_t count;
				VertexDrawMode mode;
				bool clip;
			};

			OpenGLRenderer& owner;
			std::shared_ptr<OpenGLTexture> activeTexture;
			
			typedef std::vector<BatchInfo> BatchList;
			BatchList batches;
			
			typedef std::vector<GLVertex> VertexList;
			VertexList vertices;
			
			bool clippingActive;
			RectangleF clipRect;
			
			Vector translation;
			Quaternion rotation;
			Vector pivot;
			
			mutable bool matrixValid;
			mutable glm::mat4 matrix;
		};
	}
}

#endif