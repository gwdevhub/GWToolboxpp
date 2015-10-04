#ifndef SIMPLEIMAGELOADER_PLUGINBMP_HPP
#define SIMPLEIMAGELOADER_PLUGINBMP_HPP

#include "Plugin.hpp"

namespace SimpleImageLoader
{
	class ImageData;

	class PluginBMP : public Plugin
	{
	public:
		virtual bool IsValidFormat(const RawData &data) override;

		virtual ImageData Load(Stream &data) override;
	};
}

#endif
