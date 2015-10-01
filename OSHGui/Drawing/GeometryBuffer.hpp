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
#include "Vector.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		enum class VertexDrawMode
		{
			TriangleList,
			LineList
		};

		/**
		 * Die Klasse speichert Geometrien zum Zeichnen.
		 */
		class OSHGUI_EXPORT GeometryBuffer
		{
		public:
			/**
			 * Destructor der Klasse.
			 */
			virtual ~GeometryBuffer();

			/**
			 * Legt die Position des GeometryBuffer im Raum fest.
			 *
			 * \param translation
			 */
			virtual void SetTranslation(const Vector &translation) = 0;

			/**
			 * Legt die Rotation des GeometryBuffer fest.
			 *
			 * \param rotation
			 */
			virtual void SetRotation(const Quaternion &rotation) = 0;

			/**
			 * Legt das Pivot Element (Stelle im Raum, um die der GeometryBuffer gedreht wird) fest.
			 *
			 * \param pivot
			 */
			virtual void SetPivot(const Vector &pivot) = 0;

			/**
			 * Legt die momentan aktive Textur zum Zeichnen fest.
			 *
			 * \param texture
			 */
			virtual void SetActiveTexture(const TexturePtr &texture) = 0;

			virtual void SetVertexDrawMode(VertexDrawMode mode);

			/**
			 * Legt fest, ob die anschließend hinzugefügten Vertices geclippt werden sollen.
			 *
			 * \param active
			 */
			virtual void SetClippingActive(const bool active) = 0;

			/**
			 * Ruft ab, ob derzeit Clipping aktiv ist.
			 *
			 * \return Clipping aktiv
			 */
			virtual bool IsClippingActive() const = 0;

			/**
			 * Legt die Clipping Region fest.
			 *
			 * \param region
			 */
			virtual void SetClippingRegion(const RectangleF &region) = 0;

			/**
			 * Fügt einen einzelnen Vertex zum GeometryBuffer hinzu.
			 *
			 * \param vertex
			 */
			virtual void AppendVertex(const Vertex &vertex) = 0;

			/**
			 * Fügt mehrere Vertices zum GeometryBuffer hinzu.
			 *
			 * \param vertices Zeiger auf ein Vertex Array
			 * \param count Anzahl der Vertices
			 */
			virtual void AppendGeometry(const Vertex *const vertices, uint32_t count) = 0;

			/**
			 * Zeichnet die gespeicherte Geometrie.
			 */
			virtual void Draw() const = 0;

			/**
			 * Löscht alle gespeicherten Vertices aus dem GeometryBuffer.
			 */
			virtual void Reset() = 0;

			

		protected:
			GeometryBuffer();

			VertexDrawMode drawMode;
		};

		typedef std::shared_ptr<GeometryBuffer> GeometryBufferPtr;
	}
}

#endif