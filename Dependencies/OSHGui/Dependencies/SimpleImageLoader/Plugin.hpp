#ifndef IMAGELOADER_PLUGIN_HPP
#define IMAGELOADER_PLUGIN_HPP

#include "Stream.hpp"

namespace SimpleImageLoader
{
	class ImageData;

	class Plugin
	{
	public:
		virtual bool IsValidFormat(const RawData &data) = 0;

		virtual ImageData Load(Stream &data) = 0;
	};
}

#endif
