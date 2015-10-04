#ifndef IMAGELOADER_PLUGINPNG_HPP
#define IMAGELOADER_PLUGINPNG_HPP

#include "Plugin.hpp"

namespace SimpleImageLoader
{
	class ImageData;

	class PluginPNG : public Plugin
	{
	public:
		virtual bool IsValidFormat(const RawData &data) override;

		virtual ImageData Load(Stream &data) override;
	};
}

#endif
