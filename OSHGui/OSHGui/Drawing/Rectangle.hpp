/*
* OldSchoolHack GUI
*
* by KN4CK3R http://www.oldschoolhack.me
*
* See license in OSHGui.hpp
*/

#ifndef OSHGUI_DRAWING_RECTANGLE_HPP
#define OSHGUI_DRAWING_RECTANGLE_HPP

#include "../Exports.hpp"
#include "Point.hpp"
#include "Size.hpp"

namespace OSHGui {
	namespace Drawing {
		/**
		* Speichert einen Satz von vier Zahlen, die die Position und Größe
		* eines Rechtecks angeben.
		*/
		template<typename T>
		class OSHGUI_EXPORT Rectangle {
		public:
			/**
			* Erstellt ein Rechteck ohne Ausmaße.
			*/
			Rectangle() {}

			Rectangle(Size<T> size) : size(std::move(size)) {}

			Rectangle(Point<T> location, Size<T> size) :
				location(std::move(location)),
				size(std::move(size)) {}

			Rectangle(T left, T top, T width, T height)
				: location(std::move(left), std::move(top)),
				size(std::move(width), std::move(height)) {}

			//template<typename T2>
			//operator Rectangle<T2>() const {
			//	return Rectangle<T2>(location, size);
			//}

			template<typename T2>
			Rectangle<T2> cast() const {
				return Rectangle<T2>(location.cast<T2>(), size.cast<T2>());
			}

			bool operator == (const Rectangle<T> &rhs) const {
				return location == rhs.location && size == rhs.size;
			}

			bool operator != (const Rectangle<T> &rhs) const {
				return location != rhs.location || size != rhs.size;
			}

			inline const T& GetX() const { return location.Left; }
			inline const T& GetLeft() const { return location.Left; }
			inline void SetX(const T &left) { location.Left = left; }
			inline void SetLeft(const T &left) { location.Left = left; }

			inline const T& GetY() const { return location.Top; }
			inline const T& GetTop() const { return location.Top; }
			inline void SetY(const T &top) { location.Top = top; }
			inline void SetTop(const T &top) { location.Top = top; }

			inline void SetRight(const T &right) { size.Width = right - location.Left; }
			inline T GetRight() const { return location.Left + size.Width; }

			inline void SetWidth(const T &width) { size.Width = width; }
			inline const T& GetWidth() const { return size.Width; }

			inline void SetHeight(const T &height) { size.Height = height; }
			inline const T& GetHeight() const { return size.Height; }

			inline void SetBottom(const T &bottom) { size.Height = bottom - location.Top; }
			inline T GetBottom() const { return location.Top + size.Height; }

			inline void SetLocation(const Point<T> &_location) { location = _location; }
			inline const Point<T>& GetLocation() const { return location; }

			inline void SetSize(const Size<T> &_size) { size = _size; }
			inline const Size<T>& GetSize() const { return size; }

			inline void Offset(T left, T top) { location.Offset(left, top); }
			inline void Offset(const Point<T> &offset) { location.Offset(offset); }

			Rectangle<T> OffsetEx(T left, T top) const {
				Rectangle<T> temp(*this);
				temp.Offset(left, top);
				return temp;
			}

			Rectangle<T> OffsetEx(const Point<T> &offset) const {
				Rectangle<T> temp(*this);
				temp.Offset(offset);
				return temp;
			}

			inline void Inflate(T width, T height) { size.Inflate(width, height); }
			inline void Inflate(const Size<T> &amount) { size.Inflate(amount); }

			Rectangle<T> InflateEx(T width, T height) const {
				Rectangle<T> temp(*this);
				temp.Inflate(width, height);
				return temp;
			}

			Rectangle<T> InflateEx(const Size<T> &amount) const {
				Rectangle<T> temp(*this);
				temp.Inflate(amount);
				return temp;
			}

			bool Contains(const Point<T> &point) const {
				return location.X <= point.X && point.X < GetRight() && location.Y <= point.Y && point.Y < GetBottom();
			}

			Rectangle<T> GetIntersection(const Rectangle<T>& rectangle) const {
				Rectangle<T> ret;
				if ((GetRight() > rectangle.location.X) &&
					(location.X < rectangle.GetRight()) &&
					(GetBottom() > rectangle.location.Y) &&
					(location.Y < rectangle.GetBottom())) {

					ret.SetLeft((location.X > rectangle.location.X) ? location.X : rectangle.location.X);
					ret.SetRight((GetRight() < rectangle.GetRight()) ? GetRight() : rectangle.GetRight());
					ret.SetTop((location.Y > rectangle.location.Y) ? location.Y : rectangle.location.Y);
					ret.SetBottom((GetBottom() < rectangle.GetBottom()) ? GetBottom() : rectangle.GetBottom());
				}

				return ret;
			}

			Point<T> GetTopLeft() const { return location; }
			Point<T> GetTopRight() const { return Point<T>(location.X + size.Width, location.Y); }
			Point<T> GetBottomLeft() const { return Point<T>(location.X, location.Y + size.Height); }
			Point<T> GetBottomRight() const { return Point<T>(location.X + size.Width, location.Y + size.Height); }

		private:
			Point<T> location;
			Size<T> size;
		};

		typedef Rectangle<int> RectangleI;
		typedef Rectangle<float> RectangleF;
	}
}

#endif
