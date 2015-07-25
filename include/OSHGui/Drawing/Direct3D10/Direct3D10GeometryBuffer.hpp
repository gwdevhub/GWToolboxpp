/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_DIRECT3D10GEOMETRYBUFFER_HPP
#define OSHGUI_DRAWING_DIRECT3D10GEOMETRYBUFFER_HPP

#include "../GeometryBuffer.hpp"
#include "Direct3D10Renderer.hpp"
#include <vector>

#include <d3dx10.h>

namespace OSHGui
{
	namespace Drawing
	{
		class Direct3D10Texture;

		/**
		 * Die Direct3D10 Variante des GeometryBuffer.
		 */
		class OSHGUI_EXPORT Direct3D10GeometryBuffer : public GeometryBuffer
		{
		public:
			/**
			 * Konstruktor der Klasse.
			 *
			 * \param owner Renderer Objekt, das den GeometryBuffer erstellt hat
			 */
			Direct3D10GeometryBuffer(Direct3D10Renderer &owner);
			/**
			 * Destruktor der Klasse.
			 */
			~Direct3D10GeometryBuffer();

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
			 * Updatet die Transformationsmatrix vor dem Zeichnen.
			 */
			void UpdateMatrix() const;
			/**
			 * Erledigt das Batch Management bevor neue Geometrie hinzugefügt wird.
			 */
			void PerformBatchManagement();
			/**
			 * Synchronisiert den Vertexbuffer mit der Hardware.
			 */
			void SyncHardwareBuffer() const;
			/**
			 * Erzeugt einen Vertexbuffer mit \a count Elementen.
			 * 
			 * \param count Anzahl der Vertices
			 */
			void AllocateVertexBuffer(const uint32_t count) const;
			/**
			 * Löscht die Daten aus dem Vertexbuffer.
			 */
			void CleanupVertexBuffer() const;

			struct D3DVertex
			{
				D3DVertex(float _x, float _y, float _z, DWORD _color, float _tu, float _tv)
					: x(_x),
					  y(_y),
					  z(_z),
					  color(_color),
					  tu(_tu),
					  tv(_tv)
				{

				}

				float x;
				float y;
				float z;
				DWORD color;
				float tu;
				float tv;
			};

			struct BatchInfo
			{
				BatchInfo(ID3D10ShaderResourceView *_texture, uint32_t _count, VertexDrawMode _mode, bool _clip)
					: texture(_texture),
					  count(_count),
					  mode(_mode),
					  clip(_clip)
				{

				}

				ID3D10ShaderResourceView *texture;
				uint32_t count;
				VertexDrawMode mode;
				bool clip;
			};

			Direct3D10Renderer& owner;
			std::shared_ptr<Direct3D10Texture> activeTexture;
			
			mutable ID3D10Buffer* vertexBuffer;
			mutable uint32_t bufferSize;
			mutable bool bufferSynched;

			typedef std::vector<BatchInfo> BatchList;
			BatchList batches;
			
			typedef std::vector<D3DVertex> VertexList;
			VertexList vertices;
			
			bool clippingActive;
			RectangleF clipRect;
			
			Vector translation;
			Quaternion rotation;
			Vector pivot;

			mutable bool matrixValid;
			mutable D3DXMATRIX matrix;
		};
	}
}

#endif