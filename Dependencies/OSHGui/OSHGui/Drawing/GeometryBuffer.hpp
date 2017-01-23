/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_GEOMETRYBUFFER_HPP
#define OSHGUI_DRAWING_GEOMETRYBUFFER_HPP

#include <memory>
#include "Renderer.hpp"
#include "Rectangle.hpp"
#include "Vertex.hpp"
#include "Quaternion.hpp"
#include "Point.hpp"

namespace OSHGui {
	namespace Drawing {
		enum class VertexDrawMode {
			TriangleList,
			LineList
		};

		class OSHGUI_EXPORT GeometryBuffer {
		public:
			virtual ~GeometryBuffer();

			virtual void SetTranslation(const PointF &translation) = 0;

			virtual void SetRotation(const Quaternion &rotation) = 0;

			virtual void SetPivot(const PointF &pivot) = 0;

			virtual void SetActiveTexture(const TexturePtr &texture) = 0;

			virtual void SetVertexDrawMode(VertexDrawMode mode);

			virtual void SetClippingActive(const bool active) = 0;
			virtual bool IsClippingActive() const = 0;

			virtual void SetClippingRegion(const RectangleI &region) = 0;
			virtual RectangleI GetClippingRegion() = 0;

			virtual void AppendVertex(const Vertex &vertex) = 0;
			virtual void AppendGeometry(const Vertex *const vertices, uint32_t count) = 0;

			virtual void Draw() const = 0;

			virtual void Reset() = 0;

		protected:
			GeometryBuffer();

			VertexDrawMode drawMode;
		};

		typedef std::shared_ptr<GeometryBuffer> GeometryBufferPtr;
	}
}

#endif