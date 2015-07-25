#include "ColorRectangle.hpp"
#include <utility>

namespace OSHGui
{
	namespace Drawing
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		ColorRectangle::ColorRectangle(Color color)
			: TopLeft(std::move(color)),
			  TopRight(TopLeft),
			  BottomLeft(TopLeft),
			  BottomRight(TopLeft)
		{

		}
		//---------------------------------------------------------------------------
		ColorRectangle::ColorRectangle(Color top, Color bottom)
			: TopLeft(std::move(top)),
			  TopRight(TopLeft),
			  BottomLeft(std::move(bottom)),
			  BottomRight(BottomLeft)
		{

		}
		//---------------------------------------------------------------------------
		ColorRectangle::ColorRectangle(Color topLeft, Color topRight, Color bottomLeft, Color bottomRight)
			: TopLeft(std::move(topLeft)),
			  TopRight(std::move(topRight)),
			  BottomLeft(std::move(bottomLeft)),
			  BottomRight(std::move(bottomRight))
		{

		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		bool ColorRectangle::IsMonochromatic() const
		{
			return TopLeft == TopRight && TopLeft == BottomLeft && TopLeft == BottomRight;
		}
		//---------------------------------------------------------------------------
		Color ColorRectangle::GetColorAtPoint(float x, float y) const
		{
			auto h1((TopRight - TopLeft) * x + TopLeft);
			auto h2((BottomRight - BottomLeft) * x + BottomLeft);
			return (h2 - h1) * y + h1;
		}
		//---------------------------------------------------------------------------
		//Runtime-Functions
		//---------------------------------------------------------------------------
		ColorRectangle ColorRectangle::GetSubRectangle(float left, float right, float top, float bottom) const
		{
			return ColorRectangle(
				GetColorAtPoint(left, top),
				GetColorAtPoint(right, top),
				GetColorAtPoint(left, bottom),
				GetColorAtPoint(right, bottom)
			);
		}
		//---------------------------------------------------------------------------
		ColorRectangle& ColorRectangle::operator+=(const ColorRectangle &rhs)
		{
			/*TopLeft += rhs.TopLeft;
			TopRight += rhs.TopRight;
			BottomLeft += rhs.BottomLeft;
			BottomRight += rhs.BottomRight;*/

			return *this;
		}
		//---------------------------------------------------------------------------
		ColorRectangle& ColorRectangle::operator*=(const ColorRectangle &rhs)
		{
			/*TopLeft *= rhs.TopLeft;
			TopRight *= rhs.TopRight;
			BottomLeft *= rhs.BottomLeft;
			BottomRight *= rhs.BottomRight;*/

			return *this;
		}
		//---------------------------------------------------------------------------
		ColorRectangle& ColorRectangle::operator*=(float rhs)
		{
			/*TopLeft *= rhs;
			TopRight *= rhs;
			BottomLeft *= rhs;
			BottomRight *= rhs;*/

			return *this;
		}
		//---------------------------------------------------------------------------
		const ColorRectangle operator+(const ColorRectangle &lhs, const ColorRectangle &rhs)
		{
			auto temp(lhs);
			temp += rhs;
			return temp;
		}
		//---------------------------------------------------------------------------
		const ColorRectangle operator*(const ColorRectangle &lhs, const ColorRectangle &rhs)
		{
			auto temp(lhs);
			temp *= rhs;
			return temp;
		}
		//---------------------------------------------------------------------------
		const ColorRectangle operator*(const ColorRectangle &lhs, float rhs)
		{
			auto temp(lhs);
			temp *= rhs;
			return temp;
		}
		//---------------------------------------------------------------------------
	}
}
