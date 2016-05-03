#include "RenderQueue.hpp"
#include "GeometryBuffer.hpp"
#include <algorithm>

namespace OSHGui {
	namespace Drawing {
		void RenderQueue::Draw() const {
			for (size_t i = 0; i < buffers.size(); ++i) {
				buffers[i]->Draw();
			}
		}
		//---------------------------------------------------------------------------
		void RenderQueue::AddGeometryBuffer(const GeometryBufferPtr &buffer) {
			buffers.push_back(buffer);
		}
		//---------------------------------------------------------------------------
		void RenderQueue::RemoveGeometryBuffer(const GeometryBufferPtr &buffer) {
			buffers.erase(std::remove(std::begin(buffers), std::end(buffers), buffer), std::end(buffers));
		}
		//---------------------------------------------------------------------------
		void RenderQueue::Reset() {
			buffers.clear();
		}
		//---------------------------------------------------------------------------
	}
}
