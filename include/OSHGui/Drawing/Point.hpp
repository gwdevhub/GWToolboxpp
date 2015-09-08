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

namespace OSHGui
{
	namespace Drawing
	{
		/**
		 * Represents an ordered pair of x and y coordinates as integers 
		 * is defining a point in a two-dimensional space.
		 */
		template<typename Val>
		class OSHGUI_EXPORT Point
		{
		public:
			/**
			 * Creates a point with the coordinates 0/0.
			 */
			Point()
				: X(Val()),
				  Y(Val())
			{

			}
			/**
			 * Creates a point with coordinates X / Y.
			 */
			Point(Val x, Val y)
				: X(std::move(x)),
				  Y(std::move(y))
			{

			}

			Point<Val>& operator+=(const Point<Val> &rhs)
			{
				Offset(rhs.X, rhs.Y);

				return *this;
			}

			Point<Val>& operator-=(const Point<Val> &rhs)
			{
				Offset(-rhs.X, -rhs.Y);

				return *this;
			}

			template<typename Val2>
			Point<Val>& operator*=(const std::pair<Val2, Val2> &rhs)
			{
				X *= rhs.first;
				Y *= rhs.second;

				return *this;
			}

			template<typename Val2>
			operator Point<Val2>() const
			{
				return Point<Val2>(X, Y);
			}
			
			/**
			 * Moves the point to X / Y.
			 *
			 * \param x
			 * \param y
			 */
			void Offset(Val x, Val y)
			{
				//REMOVE ME

				X += x;
				Y += y;
			}
			/**
			 * Moves the point to the offset.
			 *
			 * \param offset
			 */
			void Offset(const Point<Val> &offset)
			{
				X += offset.X;
				Y += offset.Y;
			}
			/**
			 * Copies the point and move it to X / Y.
			 *
			 * \param x
			 * \param y
			 * \return der neue Punkt
			 */
			const Point<Val> OffsetEx(Val x, Val y) const
			{
				//REMOVE ME

				auto temp(*this);
				temp.Offset(x, y);
				return temp;
			}
			/**
			 * Copies the point and moves it to the offset.
			 *
			 * \param offset
			 * \return der neue Punkt
			 */
			const Point<Val> OffsetEx(const Point<Val> &offset) const
			{
				auto temp(*this);
				temp.Offset(offset);
				return temp;
			}
			
			union {
				Val X;
				Val Left;
			};
			union {
				Val Y;
				Val Top;
			};
		};

		template<typename Val>
		bool operator==(const Point<Val> &lhs, const Point<Val> &rhs)
		{
			return lhs.X == rhs.X && lhs.Y == rhs.Y;
		}
		template<typename Val>
		bool operator!=(const Point<Val> &lhs, const Point<Val> &rhs) { return !(lhs == rhs); }
		template<typename Val>
		bool operator<(const Point<Val> &lhs, const Point<Val> &rhs)
		{
			return lhs.X < rhs.X && lhs.Y < rhs.Y;
		}
		template<typename Val>
		bool operator>(const Point<Val> &lhs, const Point<Val> &rhs) { return rhs < lhs; }
		template<typename Val>
		bool operator>=(const Point<Val> &lhs, const Point<Val> &rhs) { return !(rhs < lhs); }
		template<typename Val>
		bool operator<=(const Point<Val> &lhs, const Point<Val> &rhs) { return !(rhs > lhs); }

		template<typename Val, typename Val2>
		const Point<Val> operator+(const Point<Val> &lhs, const Point<Val2> &rhs)
		{
			auto temp(lhs);
			temp += rhs;
			return temp;
		}
		template<typename Val, typename Val2>
		const Point<Val> operator-(const Point<Val> &lhs, const Point<Val2> &rhs)
		{
			auto temp(lhs);
			temp -= rhs;
			return temp;
		}
		template<typename Val, typename Val2>
		const Point<Val> operator*(const Point<Val> &lhs, const std::pair<Val2, Val2> &rhs)
		{
			auto temp(lhs);
			temp *= rhs;
			return temp;
		}

		typedef Point<int> PointI;
		typedef Point<float> PointF;
	}
}

#endif
