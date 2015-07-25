#include "RenderQueue.hpp"
#include "GeometryBuffer.hpp"
#include <algorithm>

namespace OSHGui
{
	namespace Drawing
	{
		void RenderQueue::Draw() const
		{
			for (auto &buffer : buffers)
			{
				buffer->Draw();
			}
		}
		//---------------------------------------------------------------------------
		void RenderQueue::AddGeometryBuffer(const GeometryBufferPtr &buffer)
		{
			buffers.push_back(buffer);
		}
		//---------------------------------------------------------------------------
		void RenderQueue::RemoveGeometryBuffer(const GeometryBufferPtr &buffer)
		{
			buffers.erase(std::remove(std::begin(buffers), std::end(buffers), buffer), std::end(buffers));
		}
		//---------------------------------------------------------------------------
		void RenderQueue::Reset()
		{
			buffers.clear();
		}
		//---------------------------------------------------------------------------
	}
}
