#ifndef SIMPLEIMAGELOADER_STREAM_HPP
#define SIMPLEIMAGELOADER_STREAM_HPP

#include "SimpleImageLoader.hpp"

namespace SimpleImageLoader
{
	class Stream
	{
	public:
		enum class SeekPosition
		{
			Current,
			Begin,
			End
		};

		Stream(const RawData &data);

		void Seek(size_t offset, SeekPosition position);

		template<typename T>
		inline void Read(T *dst, size_t count)
		{
			auto size = sizeof(T)* count;
			if (offset_ + size > data_.size())
			{
				throw;
			}

			std::memcpy(dst, data_.data() + offset_, size);

			offset_ += size;
		}

		size_t Read(void *dst, size_t size, size_t count);

	private:
		const RawData &data_;
		size_t offset_;
	};
}

#endif
