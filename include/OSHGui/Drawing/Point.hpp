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
		 * Stellt ein geordnetes Paar von x- und y-Koordinaten als ganze Zahlen dar,
		 * das einen Punkt in einem zweidimensionalen Raum definiert.
		 */
		template<typename Val>
		class OSHGUI_EXPORT Point
		{
		public:
			/**
			 * Erstellt einen Punkt mit den Koordinaten 0/0.
			 */
			Point()
				: X(Val()),
				  Y(Val())
			{

			}
			/**
			 * Erstellt einen Punkt mit den Koordinaten X/Y.
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
			 * Verschiebt den Punkt um X/Y.
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
			 * Verschiebt den Punkt um das Offset.
			 *
			 * \param offset
			 */
			void Offset(const Point<Val> &offset)
			{
				X += offset.X;
				Y += offset.Y;
			}
			/**
			 * Kopiert den Punkt und verschiebt ihn um X/Y.
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
			 * Kopiert den Punkt und verschiebt ihn um das Offset.
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
			
			union
			{
				Val X;
				Val Left;
			};
			union
			{
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
