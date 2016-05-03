/*
* OldSchoolHack GUI
*
* by KN4CK3R http://www.oldschoolhack.me
*
* See license in OSHGui.hpp
*/

#ifndef OSHGUI_DRAWING_POINT_HPP
#define OSHGUI_DRAWING_POINT_HPP

#include "../Exports.hpp"
#include <utility>

namespace OSHGui {
	namespace Drawing {
		template<typename T>
		class OSHGUI_EXPORT Point {
		public:
			Point() : X(T()), Y(T()) {}
			Point(T x, T y) : X(std::move(x)), Y(std::move(y)) {}

			//// hacky wizardry to compile a bad conversion
			//template<typename T2>
			//operator Point<T2>() const {
			//	return Point<T2>(X, Y);
			//}

			template<typename T2>
			Point<T2> cast() const {
				return Point<T2>(static_cast<T2>(X), static_cast<T2>(Y));
			}

			bool operator == (const Point<T> &rhs) const {
				return X == rhs.X && Y == rhs.Y;
			}

			bool operator != (const Point<T> &rhs) const {
				return X != rhs.X || Y != rhs.Y;
			}

			const Point<T> operator + (const Point<T> &rhs) const {
				return Point<T>(X + rhs.X, Y + rhs.Y);
			}

			template<typename T2>
			const Point<T> operator + (const Point<T2> &rhs) const {
				return Point<T>(X + rhs.X, Y + rhs.Y);
			}

			template<typename T2>
			const Point<T> operator - (const Point<T2> &rhs) const {
				return Point<T>(X - rhs.X, Y - rhs.Y);
			}

			const Point<T> operator * (const std::pair<T, T> &rhs) const {
				return Point<T>(X * rhs.first, Y * rhs.second);
			}

			Point<T>& operator += (const Point<T> &rhs) {
				X += rhs.X;
				Y += rhs.Y;
				return *this;
			}

			Point<T>& operator -= (const Point<T> &rhs) {
				X -= rhs.X;
				Y -= rhs.Y;
				return *this;
			}

			Point<T>& operator *= (const std::pair<T, T> &rhs) {
				X *= rhs.first;
				Y *= rhs.second;
				return *this;
			}

			void Offset(T x, T y) {
				X += x;
				Y += y;
			}

			void Offset(const Point<T> &offset) {
				X += offset.X;
				Y += offset.Y;
			}

			const Point<T> OffsetEx(T x, T y) const {
				return Point<T>(X + x, Y + y);
			}

			const Point<T> OffsetEx(const Point<T> &offset) const {
				return Point<T>(X + offset.X, Y + offset.Y);
			}

			union {
				T X;
				T Left;
			};
			union {
				T Y;
				T Top;
			};
		};

		typedef Point<int> PointI;
		typedef Point<float> PointF;
	}
}

#endif
