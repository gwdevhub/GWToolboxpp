/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_SIZE_HPP
#define OSHGUI_DRAWING_SIZE_HPP

#include "../Exports.hpp"
#include "Point.hpp"

namespace OSHGui
{
	namespace Drawing
	{
		/**
		 * Speichert ein geordnetes Paar von ganzen Zahlen, i. d. R. die Breite
		 * und Höhe eines Rechtecks.
		 */
		template<typename Val>
		class OSHGUI_EXPORT Size
		{
		public:
			/**
			 * Erstellt ein SizeF-Objekt ohne Ausmaße.
			 */
			Size()
				: Width(Val()),
				  Height(Val())
			{

			}
			/**
			 * Erstellt ein SizeF-Objekt mit der angegeben Breite und Höhe
			 *
			 * \param width
			 * \param height
			 */
			Size(Val width, Val height)
				: Width(std::move(width)),
				  Height(std::move(height))
			{

			}
			
			Size<Val>& operator+=(const Size<Val> &rhs)
			{
				Inflate(rhs.Width, rhs.Height);

				return *this;
			}

			Size<Val>& operator-=(const Size<Val> &rhs)
			{
				Inflate(-rhs.Width, -rhs.Height);

				return *this;
			}

			template<typename Val2>
			Size<Val>& operator*=(const std::pair<Val2, Val2> &rhs)
			{
				Width *= rhs.first;
				Height *= rhs.second;

				return *this;
			}

			template<typename Val2>
			operator Size<Val2>() const
			{
				return Size<Val2>(Width, Height);
			}
			
			/**
			 * Erweitert das SizeF-Objekt um die angegebe Breite und Höhe.
			 *
			 * \param width
			 * \param height
			 */
			void Inflate(Val width, Val height)
			{
				//REMOVE ME

				Width += width;
				Height += height;
			}
			/**
			 * Erweitert das SizeF-Objekt.
			 *
			 * \param width
			 * \param height
			 */
			void Inflate(const Size<Val> &size)
			{
				Width += size.Width;
				Height += size.Height;
			}
			/**
			 * Kopiert das SizeF-Objekt und erweitert es um die angegebe Breite und Höhe.
			 *
			 * \param width
			 * \param height
			 * \return size
			 */
			Size<Val> InflateEx(Val width, Val height) const
			{
				//REMOVE ME

				auto temp(*this);
				temp.Inflate(width, height);
				return temp;
			}
			/**
			 * Kopiert das SizeF-Objekt und erweitert es.
			 *
			 * \param width
			 * \param height
			 * \return size
			 */
			Size<Val> InflateEx(const Size<Val> &size) const
			{
				auto temp(*this);
				temp.Inflate(size);
				return temp;
			}
			
			Val Width;
			Val Height;
		};

		template<typename Val>
		bool operator==(const Size<Val> &lhs, const Size<Val> &rhs)
		{
			return lhs.Width == rhs.Width && lhs.Height == rhs.Height;
		}
		template<typename Val>
		bool operator!=(const Size<Val> &lhs, const Size<Val> &rhs) { return !(lhs == rhs); }
		template<typename Val>
		bool operator<(const Size<Val> &lhs, const Size<Val> &rhs)
		{
			return lhs.Width < rhs.Width && lhs.Height < rhs.Height;
		}
		template<typename Val>
		bool operator>(const Size<Val> &lhs, const Size<Val> &rhs) { return rhs < lhs; }
		template<typename Val>
		bool operator>=(const Size<Val> &lhs, const Size<Val> &rhs) { return !(rhs < lhs); }
		template<typename Val>
		bool operator<=(const Size<Val> &lhs, const Size<Val> &rhs) { return !(rhs > lhs); }

		template<typename Val, typename Val2>
		const Size<Val> operator+(const Size<Val> &lhs, const Size<Val2> &rhs)
		{
			auto temp(lhs);
			temp += rhs;
			return temp;
		}
		template<typename Val, typename Val2>
		const Size<Val> operator-(const Size<Val> &lhs, const Size<Val2> &rhs)
		{
			auto temp(lhs);
			temp -= rhs;
			return temp;
		}
		template<typename Val, typename Val2>
		const Size<Val> operator*(const Size<Val> &lhs, const std::pair<Val2, Val2> &rhs)
		{
			auto temp(lhs);
			temp *= rhs;
			return temp;
		}

		typedef Size<int> SizeI;
		typedef Size<float> SizeF;
	}
}

#endif
