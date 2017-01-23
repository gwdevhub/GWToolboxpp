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

namespace OSHGui {
	namespace Drawing {
		template<typename T>
		class OSHGUI_EXPORT Size {
		public:
			Size() : Width(T()), Height(T()) {}
			Size(T width, T height) : Width(std::move(width)), Height(std::move(height)) {}

			// hacky wizardry to compile a bad conversion
			//template<typename T2>
			//operator Size<T2>() const {
			//	return Size<T2>(Width, Height);
			//}

			template<typename T2>
			Size<T2> cast() const {
				return Size<T2>(static_cast<T2>(Width), static_cast<T2>(Height));
			}

			bool operator == (const Size<T> &rhs) const {
				return Width == rhs.Width && Height == rhs.Height;
			}

			bool operator != (const Size<T> &rhs) const {
				return Width != rhs.Width || Height != rhs.Height;
			}

			const Size<T> operator + (const Size<T> &rhs) const {
				return Size<T>(Width + rhs.Width, Height + rhs.Height);
			}

			const Size<T> operator - (const Size<T> &rhs) const {
				return Size<T>(Width - rhs.Width, Height - rhs.Height);
			}

			const Size<T> operator * (const std::pair<T, T> &rhs) const {
				return Size<T>(Width * rhs.first, Height * rhs.second);
			}

			Size<T>& operator += (const Size<T> &rhs) {
				Width += rhs.Width;
				Height += rhs.Height;
				return *this;
			}

			Size<T>& operator -= (const Size<T> &rhs) {
				Width -= rhs.Width;
				Height -= rhs.Height;
				return *this;
			}

			void Inflate(T w, T h) {
				Width += w;
				Height += h;
			}

			void Inflate(const Size<T> &size) {
				Width += size.Width;
				Height += size.Height;
			}

			Size<T> InflateEx(T w, T h) const {
				return Size<T>(Width + w, Height + h);
			}

			Size<T> InflateEx(const Size<T> &size) const {
				return Size<T>(Width + size.Width, Height + size.Height);
			}

			T Width;
			T Height;
		};

		typedef Size<int> SizeI;
		typedef Size<unsigned long> SizeUL; // also DWORD
		typedef Size<float> SizeF;
	}
}

#endif
