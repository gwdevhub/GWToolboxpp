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

namespace OSHGui
{
	namespace Drawing
	{
		/**
		 * Speichert einen Satz von vier Zahlen, die die Position und Größe
		 * eines Rechtecks angeben.
		 */
		template<typename Val>
		class OSHGUI_EXPORT Rectangle
		{
		public:
			/**
			 * Erstellt ein Rechteck ohne Ausmaße.
			 */
			Rectangle()
			{

			}
			/**
			 * Erstellt ein Rechteckt mit den Koordinaten (0, 0, size.Width, size.Height);
			 *
			 * \param size
			 */
			Rectangle(Size<Val> size)
				: size(std::move(size))
			{

			}
			/**
			 * Erstellt ein Rechteck an der angegeben Position mit der angegeben Größe.
			 *
			 * \param location
			 * \param size
			 */
			Rectangle(Point<Val> location, Size<Val> size)
				: location(std::move(location)),
				  size(std::move(size))
			{

			}
			/**
			 * Erstellt ein Rechteck an der angegeben Position mit der angegeben Größe.
			 *
			 * \param left
			 * \param top
			 * \param width
			 * \param height
			 */
			Rectangle(Val left, Val top, Val width, Val height)
				: location(std::move(left), std::move(top)),
				  size(std::move(width), std::move(height))
			{

			}

			template<typename Val2>
			operator Rectangle<Val2>() const
			{
				return Rectangle<Val2>(location, size);
			}
			
			/**
			 * Legt die Position des linken Rands fest.
			 *
			 * \param left
			 */
			void SetLeft(const Val &left)
			{
				location.Left = left;
			}
			/**
			 * Ruft die Position des linken Rands ab.
			 *
			 * \return linker Rand
			 */
			const Val& GetLeft() const
			{
				return location.Left;
			}
			/**
			 * Legt die Position des oberen Rands fest.
			 *
			 * \param top
			 */
			void SetTop(const Val &top)
			{
				location.Top = top;
			}
			/**
			 * Ruft die Position des oberen Rands ab.
			 *
			 * \return oberer Rand
			 */
			const Val& GetTop() const
			{
				return location.Top;
			}
			/**
			 * Legt die Position des rechten Rands fest.
			 *
			 * \param right
			 */
			void SetRight(const Val &right)
			{
				size.Width = right - location.Left;
			}
			/**
			 * Ruft die Position des rechten Rands ab.
			 *
			 * \return rechter Rand
			 */
			Val GetRight() const
			{
				return location.Left + size.Width;
			}
			/**
			 * Legt die Breite des Rechtecks fest.
			 *
			 * \param width
			 */
			void SetWidth(const Val &width)
			{
				size.Width = width;
			}
			/**
			 * Ruft die Breite des Rechtecks ab.
			 *
			 * \return die Breite
			 */
			const Val& GetWidth() const
			{
				return size.Width;
			}
			/**
			 * Legt die Höhe des Rechtecks fest.
			 *
			 * \param height
			 */
			void SetHeight(const Val &height)
			{
				size.Height = height;
			}
			/**
			 * Ruft die Höhe des Rechtecks ab.
			 *
			 * \return die Höhe
			 */
			const Val& GetHeight() const
			{
				return size.Height;
			}
			/**
			 * Legt die Höhe des Rechtecks fest.
			 *
			 * \param height
			 */
			void SetBottom(const Val &bottom)
			{
				size.Height = bottom - location.Top;
			}
			/**
			 * Ruft die Höhe des Rechtecks ab.
			 *
			 * \return die Höhe
			 */
			Val GetBottom() const
			{
				return location.Top + size.Height;
			}
			/**
			 * Legt die Position des Rechtecks fest.
			 *
			 * \param _location Position des Rechtecks
			 */
			void SetLocation(const Point<Val> &_location)
			{
				location = _location;
			}
			/**
			 * Ruft die Position des Rechtecks ab.
			 *
			 * \return position
			 */
			const Point<Val>& GetLocation() const
			{
				return location;
			}
			/**
			 * Legt die Größe des Rechtecks fest.
			 *
			 * \param _size Größe des Rechtecks
			 */
			void SetSize(const Size<Val> &_size)
			{
				size = _size;
			}
			/**
			 * Ruft die Größe des Rechtecks ab.
			 *
			 * \return size
			 */
			const Size<Val>& GetSize() const
			{
				return size;
			}
			
			friend bool operator==<>(const Rectangle<Val> &lhs, const Rectangle<Val> &rhs);
			
			/**
			 * Verschiebt das Rechteck um X/Y.
			 *
			 * \param left
			 * \param top
			 */
			void Offset(Val left, Val top)
			{
				//REMOVE ME

				location.Offset(left, top);
			}
			/**
			 * Verschiebt das Rechteck um den Offset.
			 *
			 * \param p
			 */
			void Offset(const Point<Val> &offset)
			{
				location.Offset(offset);
			}
			/**
			 * Kopiert das Rechteck und verschiebt es um X/Y.
			 *
			 * \param left
			 * \param top
			 * \return rectangle
			 */
			Rectangle<Val> OffsetEx(Val left, Val top) const
			{
				//REMOVE ME

				auto temp(*this);
				temp.Offset(left, top);
				return temp;
			}
			/**
			 * Kopiert das Rechteck und verschiebt es um das Offset.
			 *
			 * \param offset
			 * \return rectangle
			 */
			Rectangle<Val> OffsetEx(const Point<Val> &offset) const
			{
				auto temp(*this);
				temp.Offset(offset);
				return temp;
			}
			/**
			 * Erweitert das RectangleF-Objekt um die angegebe Breite und Höhe.
			 *
			 * \param width
			 * \param height
			 */
			void Inflate(Val width, Val height)
			{
				size.Inflate(width, height);
			}
			/**
			 * Kopiert das RectangleF-Objekt und erweitert es um die angegebe Breite und Höhe.
			 *
			 * \param width
			 * \param height
			 * \return rectangle
			 */
			Rectangle<Val> InflateEx(Val width, Val height) const
			{
				auto temp(*this);
				temp.Inflate(width, height);
				return temp;
			}
			
			/**
			 * Gibt zurück, ob der Punkt innerhalb des Rechtecks liegt.
			 *
			 * \param point
			 * \return ja/nein
			 */
			bool Contains(const Point<Val> &point) const
			{
				return location.X <= point.X && point.X < GetRight() && location.Y <= point.Y && point.Y < GetBottom();
			}

			Rectangle<Val> GetIntersection(const Rectangle<Val>& rectangle) const
			{
				Rectangle<Val> ret;

				if ((GetRight() > rectangle.location.X) &&
					(location.X < rectangle.GetRight()) &&
					(GetBottom() > rectangle.location.Y) &&
					(location.Y < rectangle.GetBottom()))
				{
					ret.SetLeft((location.X > rectangle.location.X) ? location.X : rectangle.location.X);
					ret.SetRight((GetRight() < rectangle.GetRight()) ? GetRight() : rectangle.GetRight());
					ret.SetTop((location.Y > rectangle.location.Y) ? location.Y : rectangle.location.Y);
					ret.SetBottom((GetBottom() < rectangle.GetBottom()) ? GetBottom() : rectangle.GetBottom());
				}

				return ret;
			}

		private:
			Point<Val> location;
			Size<Val> size;
		};

		template<typename Val>
		bool operator==(const Rectangle<Val> &lhs, const Rectangle<Val> &rhs)
		{
			return lhs.location == rhs.location && lhs.size == rhs.size;
		}
		template<typename Val>
		bool operator!=(const Rectangle<Val> &lhs, const Rectangle<Val> &rhs) { return !(lhs == rhs); }

		typedef Rectangle<int> RectangleI;
		typedef Rectangle<float> RectangleF;
	}
}

#endif
