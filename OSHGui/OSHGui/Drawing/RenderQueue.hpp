/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_RENDERQUEUE_HPP
#define OSHGUI_DRAWING_RENDERQUEUE_HPP

#include <vector>
#include "GeometryBuffer.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		/**
		 * Die Klasse repräsentiert eine Queue von GeometryBuffer Objekten zum Zeichnen.
		 */
		class OSHGUI_EXPORT RenderQueue
		{
		public:
			/**
			 * Zeichnet alle GeometryBuffer Objekte und belässt sie in der Queue.
			 */
			void Draw() const;

			/**
			 * Fügt einen GeometryBuffer zur Queue hinzu.
			 *
			 * \param buffer
			 */
			void AddGeometryBuffer(const GeometryBufferPtr &buffer);

			/**
			 * Entfernt einen GeometryBuffer aus der Queue.
			 *
			 * \param buffer
			 */
			void RemoveGeometryBuffer(const GeometryBufferPtr &buffer);

			/**
			 * Entfernt alle GeometryBuffer Objekte aus der Queue.
			 */
			void Reset();

		private:
			typedef std::vector<GeometryBufferPtr> BufferList;
			BufferList buffers;
		};
	}
}

#endif
