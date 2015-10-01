#ifndef IMAGELOADER_PLUGINJPEG_HPP
#define IMAGELOADER_PLUGINJPEG_HPP

#include "Plugin.hpp"

namespace SimpleImageLoader
{
	class ImageData;

	class PluginJPEG : public Plugin
	{
	public:
		virtual bool IsValidFormat(const RawData &data) override;

		virtual ImageData Load(Stream &data) override;
	};
}

#endif
