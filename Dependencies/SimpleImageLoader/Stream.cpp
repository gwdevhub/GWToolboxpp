#include "Stream.hpp"

namespace SimpleImageLoader
{
	Stream::Stream(const RawData &data)
		: data_(data),
		  offset_(0)
	{

	}
	//---------------------------------------------------------------------------
	void Stream::Seek(size_t offset, SeekPosition position)
	{
		if (position == SeekPosition::Begin)
		{
			if (offset < 0 || (uint32_t)offset >= data_.size())
			{
				throw;
			}

			offset_ = offset;
		}
		else if (position == SeekPosition::Current)
		{
			if (offset_ + offset >= data_.size())
			{
				throw;
			}

			offset_ += offset;
		}
		else
		{
			if (data_.size() - offset >= data_.size())
			{
				throw;
			}

			offset_ = data_.size() - offset;
		}
	}
	//---------------------------------------------------------------------------
	size_t Stream::Read(void *dst, size_t size, size_t count)
	{
		auto i = 0u;
		for (; i < count; ++i)
		{
			if (offset_ + size >= data_.size())
			{
				offset_ = data_.size();
				break;
			}

			std::memcpy(dst, data_.data() + offset_, size);

			offset_ += size;
			dst = (uint8_t*)dst + size;
		}

		return i * size;
	}
	//---------------------------------------------------------------------------
}
