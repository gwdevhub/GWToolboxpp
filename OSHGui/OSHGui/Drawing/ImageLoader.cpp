/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "ImageLoader.hpp"
#include "../Misc/Exceptions.hpp"
#include <SimpleImageLoader.hpp>

namespace OSHGui
{
	namespace Drawing
	{
		ImageData LoadImageFromContainerToRGBABuffer(Misc::RawDataContainer &data)
		{
			auto image = SimpleImageLoader::LoadImage(data.GetData());

			ImageData imageData = { image.GetData(), SizeF(image.GetWidth(), image.GetHeight()) };
			
			return imageData;
		}
		//---------------------------------------------------------------------------
		ImageData LoadImageFromFileToRGBABuffer(const Misc::AnsiString &file)
		{
			Misc::RawDataContainer data;
			data.LoadFromFile(file);
			return LoadImageFromContainerToRGBABuffer(data);
		}
		//---------------------------------------------------------------------------
	}
}
