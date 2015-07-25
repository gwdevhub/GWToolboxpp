#include "CustomizableImage.hpp"
#include "../Misc/RawDataContainer.hpp"
#include "../Misc/Exceptions.hpp"
#include <algorithm>

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		CustomizableImage::CustomizableImage(SizeI size_)
			: size(std::move(size_)),
			  data(size_.Width * size_.Height)
		{

		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		const SizeI& CustomizableImage::GetSize() const
		{
			return size;
		}
		//---------------------------------------------------------------------------
		std::vector<argb_t> CustomizableImage::GetRGBAData() const
		{
			std::vector<argb_t> temp(data.size());
			auto index = 0;
			for (auto &color : data)
			{
				auto col = color.GetARGB();
				std::swap(*(((uint8_t*)&col) + 0), *(((uint8_t*)&col) + 2)); //flip R/B
				temp[index++] = col;
			}
			return temp;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		void CustomizableImage::SetRectangle(RectangleI area, const ColorRectangle &color)
		{
			if (area.GetLeft() >= size.Width || area.GetTop() >= size.Height)
			{
				return;
			}

			if (area.GetRight() > size.Width)
			{
				area.SetWidth(area.GetRight() - size.Width);
			}
			if (area.GetBottom() > size.Height)
			{
				area.SetHeight(area.GetBottom() - size.Height);
			}

			float calcWidth = std::max(area.GetWidth() - 1, 1);
			float calcHeight = std::max(area.GetHeight() - 1, 1);

			for (int j = 0; j < area.GetHeight(); ++j)
			{
				auto row = (area.GetTop() + j) * size.Width;

				for (int i = 0; i < area.GetWidth(); ++i)
				{
					auto index = (area.GetLeft() + i) + row;

					data[index] = color.GetColorAtPoint(i / calcWidth, j / calcHeight);
				}
			}
		}
		//---------------------------------------------------------------------------
	}
}
