#include "SimpleImageLoader.hpp"
#include "PluginBMP.hpp"
#include "PluginJPEG.hpp"
#include "PluginPNG.hpp"
#include "Convert.hpp"
#include "Flip.hpp"
#include <memory>

namespace SimpleImageLoader
{
	ImageData LoadImage(const RawData &data)
	{
		std::vector<std::unique_ptr<Plugin>> plugins;
		plugins.emplace_back(new PluginBMP());
		plugins.emplace_back(new PluginJPEG());
		plugins.emplace_back(new PluginPNG());

		for (auto &plugin : plugins)
		{
			if (plugin->IsValidFormat(data))
			{
				auto image = plugin->Load(Stream(data));
				ConvertTo32Bits(image);
				FlipVertical(image);
				SwapRedBlue32(image);

				return image;
			}
		}

		throw;
	}
	//---------------------------------------------------------------------------
}
